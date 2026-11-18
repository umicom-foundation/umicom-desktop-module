/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: tests/test_startup_child.c
 *
 * PURPOSE:
 *   Emit a long inert startup diagnostic and fail without creating a GUI, network request or file.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/


#include <stdio.h>
int main(void)
{
    for (unsigned i = 0U; i < 7000U; ++i) fputc('x', stderr);
    fputs(" FINAL-STARTUP-DETAIL\n", stderr);
    return 7;
}
