/*
 * fer_radio_parity.h
 *
 *  Created on: 17.04.2020
 *      Author: bertw
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * \brief       test byte for even number of 1 bits
 *
 * \param val   byte to test
 * \return      true for even
 */
bool is_bits_even(uint8_t val);

/**
 * \brief            calculate 2bit parity value for byte
 *
 * \note             The result depends on which one of the two twin-bytes it is.
 *
 * \param data_byte  data byte
 * \param pos        which position (0 or 1) in data.  words are sent in twin-pairs.
 * \return           pos0-even=0, -odd=2; pos1-even=3, -odd=1;
 */
uint8_t fer_get_word_parity(uint8_t data_byte, uint8_t pos);

/**
 * \brief            extend data byte to data word with 2 parity bits
 *
 * \note             The result depends on which one of the two twin-bytes it is.
 *
 * \param data_byte  data byte
 * \param pos        which position (0 or 1) in data.  words are sent in twin-pairs.
 * \return           10bit data word
 */
uint16_t fer_add_word_parity(uint8_t data_byte, int pos);

/**
 * \brief  test if parity according to position
 *
 * \note          The parity depends on which one of the two twin-words it is. 1,3 on even positions and 0,2 on odd positions
 * \param word    10bit data word (8 bit data, 2 pit parity)
 * \param pos     which position (0 or 1) in data.  words are sent in twin-pairs.
 * \return        ok
 */
bool fer_word_parity_p(uint16_t word, uint8_t pos);

