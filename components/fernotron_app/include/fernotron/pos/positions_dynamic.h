/**
 * \file    fernotron/pos/positions_dynamic.h
 * \brief   Keep track of positions for a currently moving shutter
 * \author: bertw
 */

#pragma once

#include "fernotron/types.h"
#include <fernotron/trx/fer_trx_c_api.h>
#include "app/config/proj_app_cfg.h"


/**
 * \brief    will be called if a moving starts or stops by any command or duration
 */
extern void (*fer_pos_POSITIONS_MOVE_cb)(bool has_moving);

// keep track of movement by simulation

/**
 * \brief         register a move related command to a shutter or group
 * \param a,g,m   the shutter(s) or alias
 * \param cmd     the command to move or stop the shutter
 * \return        -1 for error; 0 for success.
 */
int fer_simPos_registerMovingShutter(uint32_t a, uint8_t g, uint8_t m, fer_if_cmd cmd);

/**
 * \brief       register a move related command to a set of shutters or groups
 * \param mm    the shutter set
 * \param cmd   the command to move or stop the shutter
 * \return      -1 for error; 0 for success.
 */
int fer_simPos_registerMovingShutters(Fer_GmSet *mm, fer_if_cmd cmd);

/**
 * \brief        Get the current calculated position of the given shutter
 * \param a,g,m  Shutter or Alias
 * \return      -1 for error; 0 for success.
 */
int fer_simPos_getPct_whileMoving(uint32_t a, uint8_t g, uint8_t m);


/**
 * \brief          Get the duration needed to reach a given position
 * \param g,m      The shutter
 * \param curr_pct Current position
 * \param pct      Destination position to reach
 * \return         Duration in s/10
 */
uint16_t fer_simPos_calcMoveDuration_fromPctDiff_m(uint8_t g, uint8_t m, uint8_t curr_pct, uint8_t pct);

/**
 * \brief              Calculate position a moving shutter will reach after given duration
 * \param g,m          The shutter
 * direction_up        true: moving upward; false: moving downward.
 * \param duration_ts  duration in s/10
 * \return             calculated position in percent
 */
uint8_t  fer_simPos_getPct_afterDuration(uint8_t g, uint8_t m, bool direction_up, uint16_t duration_ts);


/**
 * \brief   Check for movement and if any generate events. Should be called from an interval or after being notified by callback fer_pos_POSITIONS_MOVE_cb. ???
 * \note    The events will be generated at an interval of at least 200ms. So there is no point to call this function in intervals shorter than this.
 */
void fer_pos_loopCheckMoving();

/**
 * \brief    Do work.
 */
void fer_pos_loop(void);

/**
 * \brief   Initialize this module at startup
 */
void fer_pos_init(void);

