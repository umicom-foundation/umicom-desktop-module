/*-----------------------------------------------------------------------------
 * Umicom Desk Module
 * File: src/productisation_contribution.c
 *
 * PURPOSE:
 *   Bind product identity and executable evidence to canonical Framework
 *   application definitions without copying shared implementation.
 *
 * Created by: Sammy Hegab
 * Organisation: Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#include "umicom/desktop_module/productisation_contribution.h"

static const UmiProductApplicationAdoption ADOPTION = {
    sizeof(UmiProductApplicationAdoption),
    "desktop",
    "org.umicom.desktop",
    "Umicom Desk",
    "umicom-desk-console",
    UMI_PRODUCT_FRONTEND_CONSOLE | UMI_PRODUCT_FRONTEND_GTK4,
    1,
    1,
    1,
    1
};

const UmiProductApplicationAdoption *
umi_desktop_module_productisation_contribution(void)
{
    return &ADOPTION;
}

UmiStatus umi_desktop_module_productisation_snapshot(
    UmiProductApplicationAdoptionSnapshot *out_snapshot)
{
    return umi_product_application_adoption_snapshot(
        &ADOPTION, out_snapshot);
}
