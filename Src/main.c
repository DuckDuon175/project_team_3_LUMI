#include "system_stm32f4xx.h"
#include "timer.h"
#include "eventman.h"
#include "led.h"
#include "button.h"
#include "Ucglib.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For random number generation

#define GRID_WIDTH     16
#define GRID_HEIGHT    10
#define SNAKE_MAX_LENGTH (GRID_WIDTH * GRID_HEIGHT)

#define GPIO_PIN_LOW   0
#define GPIO_PIN_HIGH  1

typedef enum {
    NO_CLICK = 0x00,
    BUTTON_PRESSED = 0x01,
    BUTTON_RELEASED = 0x02
} BUTTON_STATE;

typedef struct {
    BUTTON_STATE State;
    uint32_t timePress;
    uint32_t timeReleased;
    uint32_t Count;
} BUTTON_Name;

typedef enum {
    EVENT_EMPTY,
    EVENT_APP_INIT,
    EVENT_BUTTON_1_PRESS_LOGIC,
    EVENT_BUTTON_2_PRESS_LOGIC,
    EVENT_BUTTON_4_PRESS_LOGIC,
    EVENT_BUTTON_5_PRESS_LOGIC,
} event_api_t, *event_api_p;

typedef enum {
    STATE_APP_STARTUP,
    STATE_APP_IDLE,
    STATE_APP_GAME_OVER,
} state_app_t;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point position[SNAKE_MAX_LENGTH];
    int length;
    Point direction;
} Snake;

static ucg_t ucg;
static Snake snake;
static Point food;
static state_app_t eCurrentState;
static uint8_t gameOverFlag = 0;

void Snake_Init();
void Snake_Move();
void Snake_GenerateFood();
int Snake_CheckCollision();
void Snake_Draw();
void Snake_GameOver();

void DeviceStateMachine(uint8_t event) {
    switch(event) {
        case EVENT_BUTTON_1_PRESS_LOGIC: // Move up
            if (snake.direction.y != 1) { // Prevent moving in the opposite direction
                snake.direction.x = 0;
                snake.direction.y = -1;
            }
            break;

        case EVENT_BUTTON_2_PRESS_LOGIC: // Move left
            if (snake.direction.x != 1) { // Prevent moving in the opposite direction
                snake.direction.x = -1;
                snake.direction.y = 0;
            }
            break;

        case EVENT_BUTTON_4_PRESS_LOGIC: // Move right
            if (snake.direction.x != -1) { // Prevent moving in the opposite direction
                snake.direction.x = 1;
                snake.direction.y = 0;
            }
            break;

        case EVENT_BUTTON_5_PRESS_LOGIC: // Move down
            if (snake.direction.y != -1) { // Prevent moving in the opposite direction
                snake.direction.x = 0;
                snake.direction.y = 1;
            }
            break;

        default:
            break;
    }

    if (!gameOverFlag) {
        Snake_Move();
        if (Snake_CheckCollision()) {
            Snake_GameOver();
        }
        Snake_Draw();
    }
}

void Snake_Init() {
    snake.length = 1;
    snake.position[0].x = GRID_WIDTH / 2;
    snake.position[0].y = GRID_HEIGHT / 2;
    snake.direction.x = 0;
    snake.direction.y = -1;
    Snake_GenerateFood();
    gameOverFlag = 0;
}

void Snake_Move() {
    // Shift the snake's position array forward
    for (int i = snake.length - 1; i > 0; i--) {
        snake.position[i] = snake.position[i - 1];
    }
    // Update the snake's head position
    snake.position[0].x += snake.direction.x;
    snake.position[0].y += snake.direction.y;

    // Check if the snake eats the food
    if (snake.position[0].x == food.x && snake.position[0].y == food.y) {
        snake.length++;
        Snake_GenerateFood();
    }
}

void Snake_GenerateFood() {
    food.x = rand() % GRID_WIDTH;
    food.y = rand() % GRID_HEIGHT;
}

int Snake_CheckCollision() {
    // Check wall collision
    if (snake.position[0].x < 0 || snake.position[0].x >= GRID_WIDTH ||
        snake.position[0].y < 0 || snake.position[0].y >= GRID_HEIGHT) {
        return 1;
    }

    // Check self collision
    for (int i = 1; i < snake.length; i++) {
        if (snake.position[0].x == snake.position[i].x &&
            snake.position[0].y == snake.position[i].y) {
            return 1;
        }
    }

    return 0;
}

void Snake_Draw() {
    ucg_ClearScreen(&ucg);

    // Draw the snake
    for (int i = 0; i < snake.length; i++) {
        ucg_DrawBox(&ucg, snake.position[i].x * 8, snake.position[i].y * 8, 8, 8);
    }

    // Draw the food
    ucg_DrawBox(&ucg, food.x * 8, food.y * 8, 8, 8);
}

void Snake_GameOver() {
    gameOverFlag = 1;
    ucg_ClearScreen(&ucg);
    ucg_SetFont(&ucg, ucg_font_helvR08_tr);
    ucg_SetRotate180(&ucg);
    ucg_DrawString(&ucg, (ucg_GetWidth(&ucg) - ucg_GetStrWidth(&ucg, "Game Over")) / 2, ucg_GetHeight(&ucg) / 2, 0, "Game Over");
}

static state_app_t GetStateApp(void) {
    return eCurrentState;
}

static void SetStateApp(state_app_t state) {
    eCurrentState = state;
}

static void AppStateManager(uint8_t event) {
    switch(GetStateApp()) {
        case STATE_APP_STARTUP:
            if(event == EVENT_APP_INIT) {
                Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
                Snake_Init();
                SetStateApp(STATE_APP_IDLE);
            }
            break;
        case STATE_APP_IDLE:
            DeviceStateMachine(event);
            break;
        case STATE_APP_GAME_OVER:
            // Handle game over state if necessary
            break;
        default:
            break;
    }
}

void AppInitCommon(void) {
    SystemCoreClockUpdate();
    TimerInit();
    EventButton_Init();
    Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
    EventSchedulerInit(AppStateManager);
}

int main(void) {
    AppInitCommon();
    SetStateApp(STATE_APP_STARTUP);
    EventSchedulerAdd(EVENT_APP_INIT);
    while(1) {
        processTimerScheduler();
        processEventScheduler();
    }
}
