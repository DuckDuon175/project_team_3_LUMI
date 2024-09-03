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
#include <stdlib.h>

#define GRID_WIDTH     	  16
#define GRID_HEIGHT       10
#define SNAKE_MAX_LENGTH  (GRID_WIDTH * GRID_HEIGHT)

typedef enum {
    EVENT_EMPTY,
    EVENT_APP_INIT,
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

typedef struct {
    uint16_t x;  // Tọa độ x của mỗi cell trên thân rắn
    uint16_t y;  // Tọa độ y của mỗi cell trên thân rắn
} cell_t;

typedef struct {
    cell_t body[SNAKE_MAX_LENGTH];  // Mảng lưu các cell tạo nên thân rắn
    uint16_t length;  // Chiều dài của con rắn
    cell_t ghost;  // Cell lưu vị trí cũ của đuôi rắn trước khi di chuyển
} snake_t;

static ucg_t ucg;
static Snake snake;
static Point food;
static state_app_t eCurrentState;
static char srcScore[20]="";
static uint8_t score = 0; // Khởi tạo điểm số ban đầu là 0
static uint8_t gameOverFlag = 0;
static uint8_t isDirectionChanged = 0; // Trạng thái thay đổi hướng
static SSwTimer snakeMoveTimer = NO_TIMER;

static void LoadConfiguration(void);
void Snake_Init();
void Snake_Move();
void Snake_MoveHandler(void *args);
void Snake_GenerateFood();
int Snake_CheckCollision();
void Snake_Draw();
void Snake_GameOver();

static void LoadConfiguration(void) {
    Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
    int width = ucg_GetWidth(&ucg);
    int heightRemain = (ucg_GetHeight(&ucg) - 3) / 2;
    ucg_ClearScreen(&ucg);
    ucg_SetFont(&ucg, ucg_font_helvR10_tr);
    ucg_SetRotate180(&ucg);
    ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Snake Game")) / 2, heightRemain - 20, 0, "Snake Game");
    ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Programming by")) / 2, heightRemain - 5, 0, "Programming by");
    ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Lumi Smarthome")) / 2, heightRemain + 10, 0, "Lumi Smarthome");
    ucg_DrawString(&ucg, (width - ucg_GetStrWidth(&ucg, "Group 3")) / 2, heightRemain + 25, 0, "Group 3");
}

void DeviceStateMachine(uint8_t event) {
    if (!isDirectionChanged)
    {
        switch(event) {
        	case EVENT_OF_BUTTON_3_PRESS_LOGIC:
        	{
        		Snake_Init();
        		if (snakeMoveTimer != NO_TIMER)
        		{
        			TimerStop(snakeMoveTimer);
        			snakeMoveTimer = NO_TIMER;
        		}
        		snakeMoveTimer = TimerStart("Snake_MoveHandler", 1500, TIMER_REPEAT_FOREVER, Snake_MoveHandler, NULL);
        	}
            case EVENT_OF_BUTTON_1_PRESS_LOGIC: // Di chuyển lên
                if (snake.direction.y != 1) { // Ngăn di chuyển ngược hướng xuống
                    snake.direction.x = 0;
                    snake.direction.y = -1;
                    isDirectionChanged = 1; // Đánh dấu đã thay đổi hướng
                }
        		if (snakeMoveTimer != NO_TIMER)
        		{
        			TimerStop(snakeMoveTimer);
        			snakeMoveTimer = NO_TIMER;
        		}
        		snakeMoveTimer = TimerStart("Snake_MoveHandler", 2000, TIMER_REPEAT_FOREVER, Snake_MoveHandler, NULL);
                break;

            case EVENT_OF_BUTTON_2_PRESS_LOGIC: // Di chuyển trái
                if (snake.direction.x != 1) { // Ngăn di chuyển ngược hướng phải
                    snake.direction.x = -1;
                    snake.direction.y = 0;
                    isDirectionChanged = 1; // Đánh dấu đã thay đổi hướng
                }
        		if (snakeMoveTimer != NO_TIMER)
        		{
        			TimerStop(snakeMoveTimer);
        			snakeMoveTimer = NO_TIMER;
        		}
        		snakeMoveTimer = TimerStart("Snake_MoveHandler", 2000, TIMER_REPEAT_FOREVER, Snake_MoveHandler, NULL);
                break;

            case EVENT_OF_BUTTON_4_PRESS_LOGIC: // Di chuyển phải
                if (snake.direction.x != -1) { // Ngăn di chuyển ngược hướng trái
                    snake.direction.x = 1;
                    snake.direction.y = 0;
                    isDirectionChanged = 1; // Đánh dấu đã thay đổi hướng
                }
        		if (snakeMoveTimer != NO_TIMER)
        		{
        			TimerStop(snakeMoveTimer);
        			snakeMoveTimer = NO_TIMER;
        		}
        		snakeMoveTimer = TimerStart("Snake_MoveHandler", 2000, TIMER_REPEAT_FOREVER, Snake_MoveHandler, NULL);
                break;

            case EVENT_OF_BUTTON_5_PRESS_LOGIC: // Di chuyển xuống
                if (snake.direction.y != -1) { // Ngăn di chuyển ngược hướng lên
                    snake.direction.x = 0;
                    snake.direction.y = 1;
                    isDirectionChanged = 1; // Đánh dấu đã thay đổi hướng
                }
        		if (snakeMoveTimer != NO_TIMER)
        		{
        			TimerStop(snakeMoveTimer);
        			snakeMoveTimer = NO_TIMER;
        		}
        		snakeMoveTimer = TimerStart("Snake_MoveHandler", 2000, TIMER_REPEAT_FOREVER, Snake_MoveHandler, NULL);
                break;

            default:
                break;
        }
    }

    if (!gameOverFlag) {
        Snake_Move();
        Snake_Draw();
        if (Snake_CheckCollision()) {
            Snake_GameOver();
        }
        isDirectionChanged = 0;
    }
}

void Snake_Init() {
    snake.length = 3;

    for (int i = 0; i < snake.length; i++) {
        snake.position[i].x = GRID_WIDTH / 2;
        snake.position[i].y = GRID_HEIGHT / 2 + i;
    }

    snake.direction.x = 1;
    snake.direction.y = 0;
    Snake_GenerateFood();
    gameOverFlag = 0;
    score = 0;
}

void Snake_Move() {
    Point lastSegment = snake.position[snake.length - 1];

    for (int i = snake.length - 1; i > 0; i--) {
        snake.position[i] = snake.position[i - 1];
    }

    snake.position[0].x += snake.direction.x;
    snake.position[0].y += snake.direction.y;

    // Kiểm tra nếu rắn chạm biên và đưa nó về phía đối diện
    if (snake.position[0].x < 0) {
        snake.position[0].x = GRID_WIDTH - 1;
    } else if (snake.position[0].x >= GRID_WIDTH) {
        snake.position[0].x = 0;
    }

    if (snake.position[0].y < 0) {
        snake.position[0].y = GRID_HEIGHT + 2;
    } else if (snake.position[0].y >= GRID_HEIGHT + 3) {
        snake.position[0].y = 0;
    }

    if (snake.position[0].x == food.x && snake.position[0].y == food.y) {
        if (snake.length < SNAKE_MAX_LENGTH) {
            snake.position[snake.length] = lastSegment;
            snake.length++;
        }
        score++;
        Snake_GenerateFood();
    }
}

void Snake_MoveHandler(void *args) {
    if (!gameOverFlag) {
        Snake_Move();
        Snake_Draw();
        if (Snake_CheckCollision()) {
            Snake_GameOver();
            TimerStop(snakeMoveTimer); // Dừng bộ đếm thời gian khi game over
        }
    }
}

void Snake_GenerateFood() {
    int validPosition = 0;

    // Vòng lặp tiếp tục cho đến khi tìm được vị trí hợp lệ cho thức ăn
    while (!validPosition) {
        food.x = rand() % GRID_WIDTH;
        food.y = rand() % GRID_HEIGHT;
        validPosition = 1;

        // Kiểm tra xem thức ăn có trùng với bất kỳ phần tử nào của con rắn không
        for (int i = 0; i < snake.length; i++) {
            if (food.x == snake.position[i].x && food.y == snake.position[i].y) {
                validPosition = 0; // Nếu trùng, thiết lập lại validPosition thành 0
                break;
            }
        }
    }
}


int Snake_CheckCollision() {
    for (int i = 1; i < snake.length; i++) {
        if (snake.position[0].x == snake.position[i].x &&
            snake.position[0].y == snake.position[i].y) {
            return 1;
        }
    }

    return 0;
}

void Snake_Draw() {
	int height = ucg_GetHeight(&ucg)/10;

    ucg_ClearScreen(&ucg);
    ucg_SetRotate180(&ucg);

    ucg_SetColor(&ucg, 255, 255, 255, 255);
    ucg_DrawFrame(&ucg, 0, 0, GRID_WIDTH * 8, (GRID_HEIGHT + 3) *8);

    memset(srcScore, 0, sizeof(srcScore));
    sprintf(srcScore, "Score: %d", score);
    ucg_SetFont(&ucg, ucg_font_helvR08_tr);
    ucg_SetColor(&ucg, 255, 255, 255, 255);
    ucg_DrawString(&ucg, 0, height + 108, 0, srcScore);

    ucg_DrawBox(&ucg, food.x * 8, food.y * 8, 7, 7);

    ucg_SetColor(&ucg, 0, 0, 255, 0);
    for (int i = 0; i < snake.length; i++) {
        ucg_DrawBox(&ucg, snake.position[i].x * 8, snake.position[i].y * 8, 7, 7);
    }
}

void Snake_GameOver() {
    // Xóa toàn bộ màn hình
    ucg_ClearScreen(&ucg);

    // Đặt cờ gameOverFlag
    gameOverFlag = 1;

    ucg_SetColor(&ucg, 255, 255, 255, 255);
    ucg_DrawFrame(&ucg, 0, 0, GRID_WIDTH * 8, (GRID_HEIGHT + 3) *8);

    // Hiển thị thông báo "Game Over"
    ucg_SetFont(&ucg, ucg_font_helvR08_tr);
    ucg_SetRotate180(&ucg);

    // Hiển thị "Game Over" ở giữa màn hình
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
                LoadConfiguration();
                SetStateApp(STATE_APP_IDLE);
            }
            break;
        case STATE_APP_IDLE:
            DeviceStateMachine(event);
            break;
        case STATE_APP_GAME_OVER:
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