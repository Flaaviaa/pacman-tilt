#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// Baseado no FreeRTOSConfig.h da Aula 08, ajustado para o jogo:
// preempcao ligada, tick de 1 ms, mutexes e semaforos contadores habilitados.

#include <stdint.h>
extern uint32_t SystemCoreClock;

/* Kernel */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      ( 1000 )
#define configMAX_PRIORITIES                    ( 8 )
#define configMINIMAL_STACK_SIZE                ( 128 )
#define configMAX_TASK_NAME_LEN                 ( 12 )
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* Memoria */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   ( (size_t)(48 * 1024) )

/* Recursos */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_TIMERS                        0
#define configQUEUE_REGISTRY_SIZE               0

/* Depuracao */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configASSERT( x )                       if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* APIs incluidas */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1

/* Prioridades NVIC (STM32F4: 4 bits) */
#define configPRIO_BITS                         4
#define configKERNEL_INTERRUPT_PRIORITY         ( 15 << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 << (8 - configPRIO_BITS) )

/* Mapear handlers exigidos pelo port */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* FPU (Cortex-M4F) */
#define configENABLE_FPU                        1

#endif
