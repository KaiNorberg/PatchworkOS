#pragma once

#include "encoding/named.h"
#include "object.h"

/**
 * @brief String Conversion
 * @defgroup kernel_acpi_aml_string_conversion String Conversion
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Convert an aml data type to a string.
 *
 * @param type ACPI AML data type.
 * @return String representation of the ACPI data type or "Unknown" if it is invalid.
 */
const char* aml_type_to_string(aml_type_t type);

/**
 * @brief Convert an aml RegionSpace to a string.
 *
 * @param space ACPI AML RegionSpace.
 * @return String representation of the RegionSpace or "Unknown" if it is invalid.
 */
const char* aml_region_space_to_string(aml_region_space_t space);

/**
 * @brief Convert an aml AccessType to a string.
 *
 * @param accessType ACPI AML AccessType.
 * @return String representation of the AccessType or "Unknown" if it is invalid.
 */
const char* aml_access_type_to_string(aml_access_type_t accessType);

/**
 * @brief Convert an aml LockRule to a string.
 *
 * @param lockRule ACPI AML LockRule.
 * @return String representation of the LockRule or "Unknown" if it is invalid.
 */
const char* aml_lock_rule_to_string(aml_lock_rule_t lockRule);

/**
 * @brief Convert an aml UpdateRule to a string.
 *
 * @param updateRule ACPI AML UpdateRule.
 * @return String representation of the UpdateRule or "Unknown" if it is invalid.
 */
const char* aml_update_rule_to_string(aml_update_rule_t updateRule);

/**
 * @brief Convert an aml object to a string.
 *
 * @param object ACPI AML object.
 * @return String representation of the object or "Unknown" if it is invalid.
 */
const char* aml_object_to_string(aml_object_t* object);

/**
 * @brief Convert an aml NameString to a string.
 *
 * @param nameString ACPI AML NameString.
 * @return String representation of the NameString or "Unknown" if it is invalid.
 */
const char* aml_name_string_to_string(const aml_name_string_t* nameString);

/** @} */
