/*
 * test_alias.c
 *
 *  Created on: 27.02.2020
 *      Author: bertw
 */

#include <unity.h>
#include "fernotron/alias/pairings.h"
#include <string.h>

#include <debug/log.h>
#ifdef CONFIG_FERNOTRON_APP_DEBUG
#define DEBUG
#define DT(x) x
#define D(x) x
#else
#define DT(x)
#define D(x)
#endif
#define logtag "ferno.app"

#define A 0x101234
static Fer_GmSet gm, gme;
const Fer_GmSet gmz;
static bool succ;
#define SIZE_GM sizeof (Fer_GmSet)


static int fer_alias_and_read_back(unsigned a, uint8_t g, uint8_t m, bool unpair) {
  if (!fer_alias_controller(a, g, m, unpair))
    return -1;
  if (!fer_alias_getControllerPairings(a, &gm))
    return -2;

  gme.putMember(g, m, !unpair);
  return 0;
}

static void test_pair_multiple_members() {

  fer_alias_rmController(A);
  TEST_ASSERT_FALSE(fer_alias_getControllerPairings(A, &gm));

  gme.clear();

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 1, 2, false));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 1, 3, false));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 1, 3, true));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 4, 5, false));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 4, 6, false));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 4, 5, true));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_EQUAL(0, fer_alias_and_read_back(A, 4, 6, true));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(gme, gm, SIZE_GM);

  TEST_ASSERT_TRUE(fer_alias_rmController(A));
}



int main() {
  UNITY_BEGIN();

  RUN_TEST(test_pair_multiple_members);

  return UNITY_END();
}


#ifdef TEST_HOST
typedef int so_msg_t;
#endif

