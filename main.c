```c
/*****************************************************************************/
/*                                                                           */
/* main.c -- Dino Run game for Zybo + PmodOLED + Pmod Joystick              */
/*                                                                           */
/* OLED: 128 x 32                                                             */
/* Joystick:                                                                   */
/*   - Push joystick button = jump / restart                                 */
/*   - Left/right can be used for optional movement                           */
/*                                                                           */
/* The game uses the Digilent PmodOLED API.                                  */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include "PmodOLED.h"
#include "sleep.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"

/* ========================================================================= */
/*                         HARDWARE CONFIGURATION                            */
/* ========================================================================= */

/*
 * IMPORTANT:
 *
 * You no longer have your original xparameters.h, so the joystick GPIO
 * configuration below may need to be changed to match your Vivado design.
 *
 * The OLED addresses come from the PmodOLED design you previously used.
 *
 * For the joystick, create an AXI GPIO in Vivado and connect the joystick
 * signals to it.
 *
 * If your generated xparameters.h contains something like:
 *
 *     XPAR_AXI_GPIO_0_DEVICE_ID
 *
 * change JOYSTICK_DEVICE_ID below to:
 *
 *     XPAR_AXI_GPIO_0_DEVICE_ID
 *
 * -------------------------------------------------------------------------
 */

/* CHANGE THIS TO YOUR JOYSTICK AXI GPIO DEVICE ID */
#ifndef JOYSTICK_DEVICE_ID
#define JOYSTICK_DEVICE_ID       0
#endif

/*
 * AXI GPIO channel used for the joystick.
 *
 * Channel 1 is the normal choice if you have a single GPIO channel.
 */
#define JOYSTICK_CHANNEL         1

/*
 * Joystick input bit definitions.
 *
 * This example assumes the GPIO input is:
 *
 * bit 0 = joystick button
 * bit 1 = left
 * bit 2 = right
 * bit 3 = up
 * bit 4 = down
 *
 * If your Vivado design uses different bits, change these.
 */
#define JOY_BUTTON               0x01
#define JOY_LEFT                 0x02
#define JOY_RIGHT                0x04
#define JOY_UP                   0x08
#define JOY_DOWN                 0x10


/* ========================================================================= */
/*                         OLED CONFIGURATION                                */
/* ========================================================================= */

#define SCREEN_WIDTH             128
#define SCREEN_HEIGHT            32

#define GROUND_Y                 27

/* ========================================================================= */
/*                         GAME CONFIGURATION                                */
/* ========================================================================= */

#define DINO_X                   15

#define DINO_WIDTH               8
#define DINO_HEIGHT              10

#define DINO_GROUND_Y            (GROUND_Y - DINO_HEIGHT)

#define INITIAL_SPEED            2
#define MAX_SPEED                5

#define FRAME_DELAY_MS           45

#define GRAVITY                  1
#define JUMP_SPEED               -6

#define MAX_OBSTACLES            3

/* ========================================================================= */
/*                         DATA STRUCTURES                                   */
/* ========================================================================= */

typedef struct
{
    int x;
    int width;
    int height;
    int active;
} Obstacle;

typedef struct
{
    int x;
    int y;

    int width;
    int height;

    int velocityY;

    int jumping;
} Dino;


/* ========================================================================= */
/*                         GLOBAL VARIABLES                                  */
/* ========================================================================= */

PmodOLED myDevice;
XGpio joystick;

Dino dino;

Obstacle obstacles[MAX_OBSTACLES];

int score = 0;
int gameSpeed = INITIAL_SPEED;
int frameCounter = 0;

unsigned int randomSeed = 12345;


/* ========================================================================= */
/*                         FUNCTION PROTOTYPES                               */
/* ========================================================================= */

void DemoInitialize(void);
void DemoCleanup(void);

void GameInitialize(void);
void GameRun(void);

void DrawGame(void);
void DrawDino(void);
void DrawObstacle(Obstacle *obstacle);
void DrawGround(void);
void DrawScore(void);
void DrawGameOver(void);

void UpdateGame(void);
void UpdateDino(void);
void UpdateObstacles(void);

void SpawnObstacle(void);

int CheckCollision(Dino *d, Obstacle *o);

unsigned int RandomNumber(unsigned int max);

int ReadJoystick(void);

void Jump(void);

void EnableCaches(void);
void DisableCaches(void);


/* ========================================================================= */
/*                         MAIN                                              */
/* ========================================================================= */

int main(void)
{
    DemoInitialize();

    GameRun();

    DemoCleanup();

    while (1)
    {
        /* Never leave the program */
    }

    return 0;
}


/* ========================================================================= */
/*                         INITIALIZATION                                    */
/* ========================================================================= */

void DemoInitialize(void)
{
    EnableCaches();

    /*
     * Initialize OLED.
     *
     * These are the same PmodOLED interfaces used by your original project.
     */
    OLED_Begin(
        &myDevice,
        XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
        XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR,
        0x0,
        0x0
    );

    /*
     * Initialize joystick AXI GPIO.
     *
     * You may need to replace JOYSTICK_DEVICE_ID with the macro generated
     * by your Vivado xparameters.h.
     */
    XGpio_Initialize(
        &joystick,
        JOYSTICK_DEVICE_ID
    );

    /*
     * Set joystick channel as input.
     */
    XGpio_SetDataDirection(
        &joystick,
        JOYSTICK_CHANNEL,
        0xFFFFFFFF
    );
}


void DemoCleanup(void)
{
    OLED_End(&myDevice);

    DisableCaches();
}


/* ========================================================================= */
/*                         GAME INITIALIZATION                               */
/* ========================================================================= */

void GameInitialize(void)
{
    int i;

    score = 0;

    gameSpeed = INITIAL_SPEED;

    frameCounter = 0;

    /*
     * Initialize dino.
     */
    dino.x = DINO_X;
    dino.y = DINO_GROUND_Y;

    dino.width = DINO_WIDTH;
    dino.height = DINO_HEIGHT;

    dino.velocityY = 0;
    dino.jumping = 0;

    /*
     * Initialize obstacles.
     */
    for (i = 0; i < MAX_OBSTACLES; i++)
    {
        obstacles[i].active = 0;
        obstacles[i].x = 0;
        obstacles[i].width = 0;
        obstacles[i].height = 0;
    }

    randomSeed = 12345;

    /*
     * Start with one obstacle.
     */
    obstacles[0].active = 1;
    obstacles[0].x = 100;
    obstacles[0].width = 4;
    obstacles[0].height = 8;
}


/* ========================================================================= */
/*                         MAIN GAME LOOP                                    */
/* ========================================================================= */

void GameRun(void)
{
    int gameOver = 0;

    while (1)
    {
        /*
         * Start/restart the game.
         */
        GameInitialize();

        gameOver = 0;

        while (!gameOver)
        {
            /*
             * Read joystick.
             */
            if (ReadJoystick() & JOY_BUTTON)
            {
                Jump();
            }

            /*
             * Update game physics.
             */
            UpdateGame();

            /*
             * Check collisions.
             */
            {
                int i;

                for (i = 0; i < MAX_OBSTACLES; i++)
                {
                    if (obstacles[i].active)
                    {
                        if (CheckCollision(&dino, &obstacles[i]))
                        {
                            gameOver = 1;
                        }
                    }
                }
            }

            /*
             * Draw the new frame.
             */
            DrawGame();

            /*
             * Slow the game down enough for the OLED and MicroBlaze
             * to produce a playable frame rate.
             */
            usleep(FRAME_DELAY_MS * 1000);
        }

        /*
         * Show game-over screen.
         */
        DrawGameOver();

        /*
         * Wait for joystick button.
         */
        while (!(ReadJoystick() & JOY_BUTTON))
        {
            usleep(50000);
        }

        /*
         * Wait until button is released.
         */
        while (ReadJoystick() & JOY_BUTTON)
        {
            usleep(50000);
        }
    }
}


/* ========================================================================= */
/*                         GAME UPDATE                                       */
/* ========================================================================= */

void UpdateGame(void)
{
    UpdateDino();

    UpdateObstacles();

    frameCounter++;

    /*
     * Increase difficulty approximately every 500 frames.
     */
    if ((frameCounter % 500) == 0)
    {
        if (gameSpeed < MAX_SPEED)
        {
            gameSpeed++;
        }
    }

    /*
     * Spawn another obstacle periodically.
     */
    if ((frameCounter % 80) == 0)
    {
        SpawnObstacle();
    }
}


/* ========================================================================= */
/*                         DINO PHYSICS                                      */
/* ========================================================================= */

void UpdateDino(void)
{
    /*
     * Apply gravity.
     */
    if (dino.jumping)
    {
        dino.velocityY += GRAVITY;

        dino.y += dino.velocityY;

        /*
         * Landing.
         */
        if (dino.y >= DINO_GROUND_Y)
        {
            dino.y = DINO_GROUND_Y;

            dino.velocityY = 0;

            dino.jumping = 0;
        }
    }
}


/* ========================================================================= */
/*                         JUMP                                              */
/* ========================================================================= */

void Jump(void)
{
    /*
     * Only allow jumping while on the ground.
     */
    if (!dino.jumping)
    {
        dino.jumping = 1;

        dino.velocityY = JUMP_SPEED;
    }
}


/* ========================================================================= */
/*                         OBSTACLES                                         */
/* ========================================================================= */

void UpdateObstacles(void)
{
    int i;

    for (i = 0; i < MAX_OBSTACLES; i++)
    {
        if (obstacles[i].active)
        {
            /*
             * Move cactus to the left.
             */
            obstacles[i].x -= gameSpeed;

            /*
             * Obstacle has left the screen.
             */
            if (obstacles[i].x + obstacles[i].width < 0)
            {
                obstacles[i].active = 0;

                score++;
            }
        }
    }
}


/* ========================================================================= */
/*                         SPAWN OBSTACLE                                    */
/* ========================================================================= */

void SpawnObstacle(void)
{
    int i;

    for (i = 0; i < MAX_OBSTACLES; i++)
    {
        if (!obstacles[i].active)
        {
            obstacles[i].active = 1;

            obstacles[i].x = SCREEN_WIDTH + RandomNumber(20);

            /*
             * Random cactus sizes.
             */
            obstacles[i].width = 3 + RandomNumber(3);

            obstacles[i].height = 6 + RandomNumber(7);

            return;
        }
    }
}


/* ========================================================================= */
/*                         COLLISION DETECTION                               */
/* ========================================================================= */

int CheckCollision(Dino *d, Obstacle *o)
{
    int dLeft;
    int dRight;
    int dTop;
    int dBottom;

    int oLeft;
    int oRight;
    int oTop;
    int oBottom;

    /*
     * Dino bounding box.
     */
    dLeft = d->x;
    dRight = d->x + d->width;

    dTop = d->y;
    dBottom = d->y + d->height;

    /*
     * Obstacle bounding box.
     */
    oLeft = o->x;
    oRight = o->x + o->width;

    oTop = GROUND_Y - o->height;
    oBottom = GROUND_Y;

    /*
     * Axis-aligned bounding-box collision.
     */
    if (dRight <= oLeft)
        return 0;

    if (dLeft >= oRight)
        return 0;

    if (dBottom <= oTop)
        return 0;

    if (dTop >= oBottom)
        return 0;

    return 1;
}


/* ========================================================================= */
/*                         DRAW GAME                                         */
/* ========================================================================= */

void DrawGame(void)
{
    int i;

    /*
     * Clear framebuffer.
     */
    OLED_ClearBuffer(&myDevice);

    /*
     * Set normal drawing mode.
     */
    OLED_SetDrawColor(&myDevice, 1);
    OLED_SetDrawMode(&myDevice, OledModeSet);

    /*
     * Ground.
     */
    DrawGround();

    /*
     * Dino.
     */
    DrawDino();

    /*
     * Obstacles.
     */
    for (i = 0; i < MAX_OBSTACLES; i++)
    {
        if (obstacles[i].active)
        {
            DrawObstacle(&obstacles[i]);
        }
    }

    /*
     * Score.
     */
    DrawScore();

    /*
     * Send framebuffer to OLED.
     */
    OLED_Update(&myDevice);
}


/* ========================================================================= */
/*                         DRAW DINO                                         */
/* ========================================================================= */

void DrawDino(void)
{
    int x;
    int y;

    x = dino.x;
    y = dino.y;

    /*
     * Simple pixel-art dinosaur.
     *
     *        XX
     *       XXXX
     *      XXXXX
     *      XXXXX
     *     XXXXX
     *    XXXXX
     *    XX XX
     *    XX XX
     *
     * The drawing is intentionally simple because the OLED is only
     * 128 x 32 pixels.
     */

    /*
     * Head.
     */
    OLED_MoveTo(&myDevice, x + 3, y);
    OLED_FillRect(&myDevice, x + 7, y + 3);

    /*
     * Body.
     */
    OLED_MoveTo(&myDevice, x + 1, y + 3);
    OLED_FillRect(&myDevice, x + 6, y + 7);

    /*
     * Tail.
     */
    OLED_MoveTo(&myDevice, x - 2, y + 5);
    OLED_LineTo(&myDevice, x + 2, y + 3);

    /*
     * Legs.
     */
    OLED_MoveTo(&myDevice, x + 2, y + 7);
    OLED_LineTo(&myDevice, x + 2, y + 9);

    OLED_MoveTo(&myDevice, x + 6, y + 7);
    OLED_LineTo(&myDevice, x + 6, y + 9);

    /*
     * Eye.
     */
    OLED_SetDrawColor(&myDevice, 0);

    OLED_MoveTo(&myDevice, x + 6, y + 1);
    OLED_FillRect(&myDevice, x + 6, y + 1);

    /*
     * Restore drawing color.
     */
    OLED_SetDrawColor(&myDevice, 1);
}


/* ========================================================================= */
/*                         DRAW OBSTACLE                                     */
/* ========================================================================= */

void DrawObstacle(Obstacle *obstacle)
{
    int x;
    int bottom;
    int top;

    x = obstacle->x;

    bottom = GROUND_Y;

    top = bottom - obstacle->height;

    /*
     * Main cactus.
     */
    OLED_MoveTo(
        &myDevice,
        x,
        top
    );

    OLED_FillRect(
        &myDevice,
        x + obstacle->width,
        bottom
    );

    /*
     * Small cactus arms.
     */
    if (obstacle->height >= 8)
    {
        /*
         * Left arm.
         */
        OLED_MoveTo(
            &myDevice,
            x - 2,
            top + 4
        );

        OLED_LineTo(
            &myDevice,
            x,
            top + 4
        );

        OLED_LineTo(
            &myDevice,
            x - 2,
            top + 2
        );

        /*
         * Right arm.
         */
        OLED_MoveTo(
            &myDevice,
            x + obstacle->width,
            top + 5
        );

        OLED_LineTo(
            &myDevice,
            x + obstacle->width + 2,
            top + 5
        );

        OLED_LineTo(
            &myDevice,
            x + obstacle->width + 2,
            top + 3
        );
    }
}


/* ========================================================================= */
/*                         DRAW GROUND                                       */
/* ========================================================================= */

void DrawGround(void)
{
    OLED_MoveTo(
        &myDevice,
        0,
        GROUND_Y
    );

    OLED_LineTo(
        &myDevice,
        SCREEN_WIDTH - 1,
        GROUND_Y
    );
}


/* ========================================================================= */
/*                         DRAW SCORE                                        */
/* ========================================================================= */

void DrawScore(void)
{
    /*
     * The PmodOLED character routines can be used here if desired.
     *
     * For maximum compatibility with the supplied OLED graphics driver,
     * this simple implementation displays a small score bar instead.
     *
     * Every five points adds another small pixel block.
     */

    int blocks;
    int i;

    blocks = score / 5;

    if (blocks > 20)
    {
        blocks = 20;
    }

    for (i = 0; i < blocks; i++)
    {
        OLED_MoveTo(
            &myDevice,
            100 + i,
            1
        );

        OLED_FillRect(
            &myDevice,
            100 + i,
            2
        );
    }
}


/* ========================================================================= */
/*                         GAME OVER SCREEN                                  */
/* ========================================================================= */

void DrawGameOver(void)
{
    int i;

    OLED_ClearBuffer(&myDevice);

    OLED_SetDrawColor(&myDevice, 1);
    OLED_SetDrawMode(&myDevice, OledModeSet);

    /*
     * Border.
     */
    OLED_MoveTo(
        &myDevice,
        0,
        0
    );

    OLED_DrawRect(
        &myDevice,
        SCREEN_WIDTH - 1,
        SCREEN_HEIGHT - 1
    );

    /*
     * Game-over graphic.
     *
     * Large X.
     */
    for (i = 0; i < 8; i++)
    {
        OLED_MoveTo(
            &myDevice,
            45 + i,
            10 + i
        );

        OLED_LineTo(
            &myDevice,
            53 - i,
            18 - i
        );

        OLED_MoveTo(
            &myDevice,
            53 - i,
            10 + i
        );

        OLED_LineTo(
            &myDevice,
            45 + i,
            18 - i
        );
    }

    /*
     * Score indicator.
     */
    for (i = 0; i < score && i < 40; i++)
    {
        OLED_MoveTo(
            &myDevice,
            44 + i,
            25
        );

        OLED_FillRect(
            &myDevice,
            44 + i,
            26
        );
    }

    OLED_Update(&myDevice);
}


/* ========================================================================= */
/*                         JOYSTICK                                          */
/* ========================================================================= */

int ReadJoystick(void)
{
    unsigned int value;

    /*
     * Read joystick GPIO.
     */
    value = XGpio_DiscreteRead(
        &joystick,
        JOYSTICK_CHANNEL
    );

    return (int)value;
}


/* ========================================================================= */
/*                         RANDOM NUMBER                                     */
/* ========================================================================= */

unsigned int RandomNumber(unsigned int max)
{
    /*
     * Simple linear congruential generator.
     *
     * We use this instead of rand() so that the game doesn't require
     * additional libraries.
     */
    randomSeed =
        randomSeed * 1103515245 +
        12345;

    if (max == 0)
    {
        return 0;
    }

    return (randomSeed >> 16) % max;
}


/* ========================================================================= */
/*                         CACHE FUNCTIONS                                   */
/* ========================================================================= */

void EnableCaches(void)
{
#ifdef __MICROBLAZE__

#ifdef XPAR_MICROBLAZE_USE_ICACHE
    Xil_ICacheEnable();
#endif

#ifdef XPAR_MICROBLAZE_USE_DCACHE
    Xil_DCacheEnable();
#endif

#endif
}


void DisableCaches(void)
{
#ifdef __MICROBLAZE__

#ifdef XPAR_MICROBLAZE_USE_DCACHE
    Xil_DCacheDisable();
#endif

#ifdef XPAR_MICROBLAZE_USE_ICACHE
    Xil_ICacheDisable();
#endif

#endif
}
```
