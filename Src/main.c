#include "system_stm32f4xx.h"
#include "timer.h"
#include "eventman.h"
#include "led.h"
#include "eventbutton.h"
#include "button.h"
#include "Ucglib.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For random number generation

#define GRID_WIDTH     16
#define GRID_HEIGHT    10
#define SNAKE_MAX_LENGTH (GRID_WIDTH * GRID_HEIGHT)
#define CELL_WIDTH     10  // Độ rộng của mỗi ô trên lưới
#define CELL_HEIGHT    10  // Chiều cao của mỗi ô trên lưới

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
static char srcScore[20]="";
static uint8_t score = 0; // Khởi tạo điểm số ban đầu là 0
static uint8_t gameOverFlag = 0;
static void LoadConfiguration(void);
void Snake_Init();
void Snake_Move();
void Snake_GenerateFood();
int Snake_CheckCollision();
void Snake_Draw();
void Snake_GameOver();

static void LoadConfiguration(void) {
	Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
	int width = ucg_GetWidth(&ucg);
	int heightRemain = (ucg_GetHeight(&ucg) - 3)/2;
	ucg_ClearScreen(&ucg);
	ucg_SetFont(&ucg, ucg_font_helvR08_tr);
	ucg_SetRotate180(&ucg);
	ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Snake Game"))/2, heightRemain - 14, 0, "Snake Game");
	ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Programming by"))/2, heightRemain, 0, "Programming by");
	ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Lumi Smarthome"))/2, heightRemain + 14, 0, "Lumi Smarthome");
	ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Group 3"))/2, heightRemain + 28, 0, "Group 3");
}
void DeviceStateMachine(uint8_t event) {
    switch(event) {
        case EVENT_BUTTON_1_PRESS_LOGIC: // Di chuyển lên
            if (snake.direction.y != 1) { // Ngăn di chuyển ngược hướng xuống
                snake.direction.x = 0;
                snake.direction.y = -1;
            }
            break;

        case EVENT_BUTTON_2_PRESS_LOGIC: // Di chuyển trái
            if (snake.direction.x != 1) { // Ngăn di chuyển ngược hướng phải
                snake.direction.x = -1;
                snake.direction.y = 0;
            }
            break;

        case EVENT_BUTTON_4_PRESS_LOGIC: // Di chuyển phải
            if (snake.direction.x != -1) { // Ngăn di chuyển ngược hướng trái
                snake.direction.x = 1;
                snake.direction.y = 0;
            }
            break;

        case EVENT_BUTTON_5_PRESS_LOGIC: // Di chuyển xuống
            if (snake.direction.y != -1) { // Ngăn di chuyển ngược hướng lên
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
    snake.length = 4;
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
        score++;
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
    ucg_SetFont(&ucg, ucg_font_helvR08_tr);
    ucg_SetRotate180(&ucg);
    // Draw the snake
    for (int i = 0; i < snake.length; i++) {
        ucg_DrawBox(&ucg, snake.position[i].x * 8, snake.position[i].y * 8, 8, 8);
    }

    // Draw the food
    ucg_DrawBox(&ucg, food.x * 8, food.y * 8, 8, 8);

    //Draw the frame
    ucg_DrawFrame(&ucg, 0, 0, GRID_WIDTH * CELL_WIDTH, GRID_HEIGHT * CELL_HEIGHT);

    //Score
    int height = ucg_GetHeight(&ucg)/10;
	memset(srcScore, 0, sizeof(srcScore));
	sprintf(srcScore, " Score: %d", score);
	ucg_DrawString(&ucg, 0, height + 108, 0, srcScore);
}

void Snake_GameOver() {
    gameOverFlag = 1;

    // Clear the entire screen
    ucg_ClearScreen(&ucg);

    // Display the "Game Over" message
    ucg_SetFont(&ucg, ucg_font_helvR08_tr);
    ucg_SetRotate180(&ucg);

    // Hiển thị "Game Over" ở giữa màn hình
    ucg_DrawString(&ucg, (ucg_GetWidth(&ucg) - ucg_GetStrWidth(&ucg, "Game Over")) / 2, ucg_GetHeight(&ucg) / 2, 0, "Game Over");

    // Hiển thị điểm số cuối cùng ngay dưới "Game Over"
    ucg_DrawString(&ucg, (ucg_GetWidth(&ucg) - ucg_GetStrWidth(&ucg, srcScore)) / 2, (ucg_GetHeight(&ucg) / 2) + 14, 0, srcScore);

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
                LoadConfiguration();
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
