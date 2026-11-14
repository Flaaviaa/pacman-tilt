#include "stm32f4xx.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "board.h"
#include "buttons.h"
#include "game.h"

// ---------------------------------------------------------------------------
// Pac-Tilt - ELF74 Projeto Final
//
// Prioridades (maior numero = mais prioritario), justificativa no README:
//   5  imu        amostragem periodica de 10 ms; curta, nao pode atrasar
//   4  botoes     debounce de 10 ms; curta
//   4  usb        a pilha USB precisa ser atendida a cada ~1 ms
//   3  jogo       relogio de 20 ms e maquina de estados (regras, colisoes)
//   2  pacman     fisica, liberada pelo jogo a cada quadro
//   2  fantasma*4 IA, mesma prioridade (round-robin entre si)
//   1  display    a mais demorada; usa o tempo que sobra. Se atrasar, so
//                 cai a taxa de quadros: a logica do jogo nao e afetada.
// ---------------------------------------------------------------------------

game_t g_game;

SemaphoreHandle_t m_game;
SemaphoreHandle_t m_lcd;
SemaphoreHandle_t m_i2c;
SemaphoreHandle_t s_step[NUM_CHARACTERS];
SemaphoreHandle_t s_frame;

QueueHandle_t q_tilt;
QueueHandle_t q_input;
QueueHandle_t q_game_evt;
QueueHandle_t q_render;
QueueHandle_t q_log;

TaskHandle_t h_imu;
TaskHandle_t h_tasks[10];   // para o comando 's' (uso de pilha)

static void fatal(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;)
    {
        led_toggle();
        busy_delay_ms(80);
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    fatal();
}

void vApplicationMallocFailedHook(void)
{
    fatal();
}

static void create_task(TaskFunction_t fn, const char *name, uint16_t stack,
                        void *arg, UBaseType_t prio, int slot)
{
    if (xTaskCreate(fn, name, stack, arg, prio, &h_tasks[slot]) != pdPASS)
        fatal();
}

int main(void)
{
    board_check_dfu_request();

    clock_init();
    NVIC_SetPriorityGrouping(3U);   // 4 bits de preempcao, como o FreeRTOS espera

    led_init();
    buttons_init();

    m_game = xSemaphoreCreateMutex();
    m_lcd  = xSemaphoreCreateMutex();
    m_i2c  = xSemaphoreCreateMutex();
    s_frame = xSemaphoreCreateBinary();

    for (int i = 0; i < NUM_CHARACTERS; i++)
        s_step[i] = xSemaphoreCreateBinary();

    q_tilt     = xQueueCreate(1, sizeof(tilt_msg_t));
    q_input    = xQueueCreate(8, sizeof(input_msg_t));
    q_game_evt = xQueueCreate(16, sizeof(game_event_t));
    q_render   = xQueueCreate(8, sizeof(render_msg_t));
    q_log      = xQueueCreate(16, 96);

    create_task(task_usb,     "usb",     512, NULL, 4, 0);
    create_task(task_imu,     "imu",     512, NULL, 5, 1);
    create_task(task_buttons, "botoes",  384, NULL, 4, 2);
    create_task(task_game,    "jogo",    512, NULL, 3, 3);
    create_task(task_pacman,  "pacman",  384, NULL, 2, 4);

    for (int i = 0; i < MAX_GHOSTS; i++)
        create_task(task_ghost, "fantasma", 384, (void *)(intptr_t)i, 2, 5 + i);

    create_task(task_display, "display", 640, NULL, 1, 9);

    h_imu = h_tasks[1];
    led_set(1);

    vTaskStartScheduler();

    fatal();
    return 0;
}
