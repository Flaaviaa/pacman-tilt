#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx.h"
#include "tusb.h"

#include "game.h"
#include "config.h"
#include "board.h"
#include "mpu6050.h"

// Tarefas de perifericos de entrada e da serial USB:
//   task_imu     - le o MPU6050 a cada IMU_MS e publica a inclinacao (q_tilt)
//   task_buttons - debounce dos botoes e envio de eventos (q_input)
//   task_usb     - atende a pilha USB e escreve os logs (q_log)

#define LOG_LINE 96

typedef struct
{
    char text[LOG_LINE];
} log_msg_t;

static volatile uint8_t stream_tilt = 0;

void log_printf(const char *fmt, ...)
{
    log_msg_t m;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(m.text, sizeof(m.text), fmt, ap);
    va_end(ap);

    xQueueSend(q_log, &m, 0);
}

// ---------------------------------------------------------------------------
// IMU
// ---------------------------------------------------------------------------
#define CAL_SAMPLES 32
#define LPF_ALPHA   0.35f

static int imu_read(mpu_raw_t *raw)
{
    int r;

    xSemaphoreTake(m_i2c, portMAX_DELAY);
    // A leitura de varios bytes no I2C do F4 tem que liberar o ACK no
    // tempo certo; uma preempcao no meio pode estragar a transacao.
    taskENTER_CRITICAL();
    r = mpu_read_accel(raw);
    taskEXIT_CRITICAL();
    xSemaphoreGive(m_i2c);

    return r;
}

static int imu_init(void)
{
    int who;

    xSemaphoreTake(m_i2c, portMAX_DELAY);
    who = mpu_init();
    xSemaphoreGive(m_i2c);

    xSemaphoreTake(m_game, portMAX_DELAY);
    g_game.imu_ok = (uint8_t)(who >= 0);
    xSemaphoreGive(m_game);

    if (who < 0)
        log_printf("IMU: MPU6050 nao responde (PB6=SCL, PB7=SDA)\r\n");
    else
        log_printf("IMU: MPU6050 ok, WHO_AM_I=0x%02X\r\n", who);

    return who >= 0;
}

void task_imu(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    mpu_raw_t raw;
    float off_x = 0.0f, off_y = 0.0f;
    float acc_x = 0.0f, acc_y = 0.0f;
    float fx = 0.0f, fy = 0.0f;
    int cal_left = 0;
    int errors = 0;
    uint32_t n = 0;
    int ok = imu_init();

    for (;;)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(IMU_MS));
        n++;

        if (!ok)
        {
            // Tenta de novo a cada 1 s (sensor desconectado)
            if ((n % (1000U / IMU_MS)) == 0U)
                ok = imu_init();
            continue;
        }

        // Pedido de calibracao vindo do jogo (inicio de nivel / apos pausa)
        if (ulTaskNotifyTake(pdTRUE, 0) > 0U)
        {
            cal_left = CAL_SAMPLES;
            acc_x = acc_y = 0.0f;
        }

        if (imu_read(&raw) != 0)
        {
            if (++errors > 20)
            {
                ok = 0;
                errors = 0;
                xSemaphoreTake(m_game, portMAX_DELAY);
                g_game.imu_ok = 0;
                xSemaphoreGive(m_game);
                log_printf("IMU: falhas seguidas, reiniciando\r\n");
            }
            continue;
        }

        errors = 0;

        float x = raw.ax / 16384.0f;
        float y = raw.ay / 16384.0f;

#if IMU_SWAP_XY
        float t = x; x = y; y = t;
#endif
#if IMU_INVERT_X
        x = -x;
#endif
#if IMU_INVERT_Y
        y = -y;
#endif

        if (cal_left > 0)
        {
            // Placa parada na posicao de jogo: essa inclinacao vira o zero
            acc_x += x;
            acc_y += y;

            if (--cal_left == 0)
            {
                off_x = acc_x / CAL_SAMPLES;
                off_y = acc_y / CAL_SAMPLES;
                fx = fy = 0.0f;
                log_printf("IMU: calibrada, zero = (%d, %d) mg\r\n",
                           (int)(off_x * 1000.0f), (int)(off_y * 1000.0f));
            }
            continue;
        }

        // Filtro passa-baixas de 1a ordem para tirar a vibracao da mao
        fx += LPF_ALPHA * ((x - off_x) - fx);
        fy += LPF_ALPHA * ((y - off_y) - fy);

        tilt_msg_t msg = { fx, fy };
        xQueueOverwrite(q_tilt, &msg);

        if (stream_tilt && (n % 10U) == 0U)
            log_printf("tilt x=%5d y=%5d mg\r\n", (int)(fx * 1000.0f), (int)(fy * 1000.0f));
    }
}

// ---------------------------------------------------------------------------
// Botoes
// ---------------------------------------------------------------------------
#define DEBOUNCE_SAMPLES 3      // 3 x 10 ms estaveis

void task_buttons(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t stable = 0, candidate = 0;
    int count = 0;

    for (;;)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(BUTTON_MS));

        uint32_t raw = buttons_read_raw();

        if (raw != candidate)
        {
            candidate = raw;
            count = 0;
            continue;
        }

        if (count < DEBOUNCE_SAMPLES && ++count == DEBOUNCE_SAMPLES)
        {
            uint32_t pressed = candidate & ~stable;

            for (int i = 0; i < BTN_COUNT; i++)
            {
                if (pressed & (1U << i))
                {
                    input_msg_t m = { (button_id_t)i };
                    xQueueSend(q_input, &m, 0);
                    log_printf("botao %c\r\n", "ABK"[i]);
                }
            }

            stable = candidate;
        }
    }
}

// ---------------------------------------------------------------------------
// USB CDC (TinyUSB) - logs e comandos simples
// ---------------------------------------------------------------------------
static volatile uint8_t dfu_request = 0;

uint32_t tusb_time_millis_api(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void tusb_time_delay_ms_api(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void OTG_FS_IRQHandler(void)
{
    tusb_int_handler(0, true);
}

// Abrir a serial a 1200 baud reinicia a placa no bootloader DFU
// (tools/upload.ps1 usa isso para gravar sem apertar BOOT0)
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *coding)
{
    (void)itf;

    if (coding->bit_rate == 1200U)
        dfu_request = 1;
}

static void usb_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    __DSB();

    // PA11 = DM e PA12 = DP, AF10
    GPIOA->MODER &= ~((3U << (11U * 2U)) | (3U << (12U * 2U)));
    GPIOA->MODER |=  (2U << (11U * 2U)) | (2U << (12U * 2U));
    GPIOA->OSPEEDR |= (3U << (11U * 2U)) | (3U << (12U * 2U));
    GPIOA->AFR[1] &= ~((0xFU << 12U) | (0xFU << 16U));
    GPIOA->AFR[1] |=  (10U << 12U) | (10U << 16U);

    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    __DSB();
}

static void print_stats(void)
{
    static const char *const names[] = { "usb", "imu", "botoes", "jogo", "pacman",
                                         "fant0", "fant1", "fant2", "fant3", "display" };
    extern TaskHandle_t h_tasks[10];

    log_printf("heap livre: %u bytes\r\n", (unsigned)xPortGetFreeHeapSize());

    for (int i = 0; i < 10; i++)
        if (h_tasks[i] != NULL)
            log_printf("  %-8s pilha livre: %u palavras\r\n", names[i],
                       (unsigned)uxTaskGetStackHighWaterMark(h_tasks[i]));
}

static void handle_command(char c)
{
    switch (c)
    {
    case 't': stream_tilt = !stream_tilt; break;
    case 's': print_stats(); break;
    case 'h':
    case '?':
        log_printf("comandos: t = liga/desliga leituras da IMU, s = pilhas/heap\r\n");
        break;
    default: break;
    }
}

void task_usb(void *arg)
{
    (void)arg;
    log_msg_t m;
    uint8_t was_connected = 0;
    tusb_rhport_init_t init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };

    usb_gpio_init();
    tusb_rhport_init(0, &init);
    NVIC_SetPriority(OTG_FS_IRQn, 6U);
    NVIC_EnableIRQ(OTG_FS_IRQn);

    for (;;)
    {
        tud_task();

        if (dfu_request)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            board_reboot_to_dfu();
        }

        uint8_t connected = tud_cdc_connected();

        if (connected && !was_connected)
            log_printf("Pac-Tilt ELF74 - digite h para ajuda\r\n");
        was_connected = connected;

        while (tud_cdc_available())
        {
            uint8_t c;
            if (tud_cdc_read(&c, 1) == 1)
                handle_command((char)c);
        }

        // Escreve os logs pendentes (sem terminal aberto eles sao descartados)
        while (uxQueueMessagesWaiting(q_log) > 0U &&
               (!connected || tud_cdc_write_available() >= LOG_LINE))
        {
            xQueueReceive(q_log, &m, 0);

            if (connected)
                tud_cdc_write_str(m.text);
        }

        if (connected)
            tud_cdc_write_flush();

        vTaskDelay(1);
    }
}
