#pragma once

#include "aml.h"
#include "encoding/named.h"

/**
 * @brief ACPI AML String Conversion
 * @defgroup kernel_acpi_aml_string_conversion String Conversion
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Convert an aml node type to a string.
 *
 * @param type ACPI AML node type.
 * @return String representation of the ACPI node type or "Unknown" if it is invalid.
 */
const char* aml_node_type_to_string(aml_node_type_t type);

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
 * @brief Convert an aml DataObject to a string.
 *
 * @param dataObject ACPI AML DataObject.
 * @return String representation of the DataObject or "Unknown" if it is invalid.
 */
const char* aml_data_object_to_string(aml_data_object_t* dataObject);

/**
 * @brief Convert an aml DataType to a string.
 *
 * @param type ACPI AML DataType.
 * @return String representation of the DataType or "Unknown" if it is invalid.
 */
const char* aml_data_type_to_string(aml_data_type_t type);

/**
 * @brief Convert an aml NameString to a string.
 *
 * @param nameString ACPI AML NameString.
 * @return String representation of the NameString or "Unknown" if it is invalid.
 */
const char* aml_name_string_to_string(aml_name_string_t* nameString);

/** @} */
