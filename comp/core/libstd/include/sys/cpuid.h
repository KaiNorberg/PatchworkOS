#ifndef _SYS_CPUID_H
#define _SYS_CPUID_H 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/defs.h>

/**
 * @brief CPU feature detection
 * @ingroup libstd
 * @defgroup libstd_sys_cpuid CPUID
 *
 * The `sys/cpuid.h` header provides functions for detecting CPU features using the CPUID instruction.
 *
 * @see [CPUID Instruction](https://www.felixcloutier.com/x86/cpuid)
 *
 * @{
 */

/**
 * @brief Input EAX values.
 * @enum cpuid_input_eax_t
 */
typedef enum
{
    CPUID_EAX_NONE = 0x00,
    CPUID_EAX_FEATURE_INFO = 0x01,
    CPUID_EAX_EXTENDED_FEATURE_INFO = 0x07,
} cpuid_input_eax_t;

/**
 * @brief Input ECX values.
 * @enum cpuid_input_ecx_t
 */
typedef enum
{
    CPUID_ECX_NONE = 0x00,
} cpuid_input_ecx_t;

/**
 * @brief ECX feature flags.
 * @enum cpuid_ecx_features_t
 *
 * These flags are returned in the ECX register after calling the CPUID instruction with EAX=CPUID_EAX_FEATURE_INFO.
 */
typedef enum
{
    CPUID_ECX_SSE3 = 1 << 0,
    CPUID_ECX_PCLMULQDQ = 1 << 1,
    CPUID_ECX_DTES64 = 1 << 2,
    CPUID_ECX_MONITOR = 1 << 3,
    CPUID_ECX_DS_CPL = 1 << 4,
    CPUID_ECX_VMX = 1 << 5,
    CPUID_ECX_SMX = 1 << 6,
    CPUID_ECX_EIST = 1 << 7,
    CPUID_ECX_TM2 = 1 << 8,
    CPUID_ECX_SSSE3 = 1 << 9,
    CPUID_ECX_CNXT_ID = 1 << 10,
    CPUID_ECX_SDBG = 1 << 11,
    CPUID_ECX_FMA = 1 << 12,
    CPUID_ECX_CMPXCHG16B = 1 << 13,
    CPUID_ECX_XTPR_UPDATE_CONTROL = 1 << 14,
    CPUID_ECX_PDCM = 1 << 15,
    CPUID_ECX_PCID = 1 << 17,
    CPUID_ECX_DCA = 1 << 18,
    CPUID_ECX_SSE4_1 = 1 << 19,
    CPUID_ECX_SSE4_2 = 1 << 20,
    CPUID_ECX_X2APIC = 1 << 21,
    CPUID_ECX_MOVBE = 1 << 22,
    CPUID_ECX_POPCNT = 1 << 23,
    CPUID_ECX_TSC_DEADLINE = 1 << 24,
    CPUID_ECX_AESNI = 1 << 25,
    CPUID_ECX_XSAVE = 1 << 26,
    CPUID_ECX_OSXSAVE = 1 << 27,
    CPUID_ECX_AVX = 1 << 28,
    CPUID_ECX_F16C = 1 << 29,
    CPUID_ECX_RDRAND = 1 << 30,
} cpuid_ecx_features_t;

/**
 * @brief EBX feature flags.
 * @enum cpuid_ebx_features_t
 *
 * These flags are returned in the EBX register after calling the CPUID instruction with EAX=CPUID_FEATURE_EXTENDED_ID.
 */
typedef enum
{
    CPUID_EDX_FPU = 1 << 0,
    CPUID_EDX_VME = 1 << 1,
    CPUID_EDX_DE = 1 << 2,
    CPUID_EDX_PSE = 1 << 3,
    CPUID_EDX_TSC = 1 << 4,
    CPUID_EDX_MSR = 1 << 5,
    CPUID_EDX_PAE = 1 << 6,
    CPUID_EDX_MCE = 1 << 7,
    CPUID_EDX_CX8 = 1 << 8,
    CPUID_EDX_APIC = 1 << 9,
    CPUID_EDX_SEP = 1 << 11,
    CPUID_EDX_MTRR = 1 << 12,
    CPUID_EDX_PGE = 1 << 13,
    CPUID_EDX_MCA = 1 << 14,
    CPUID_EDX_CMOV = 1 << 15,
    CPUID_EDX_PAT = 1 << 16,
    CPUID_EDX_PSE36 = 1 << 17,
    CPUID_EDX_PSN = 1 << 18,
    CPUID_EDX_CLFSH = 1 << 19,
    CPUID_EDX_RESERVED1 = 1 << 20,
    CPUID_EDX_DS = 1 << 21,
    CPUID_EDX_ACPI = 1 << 22,
    CPUID_EDX_MMX = 1 << 23,
    CPUID_EDX_FXSR = 1 << 24,
    CPUID_EDX_SSE = 1 << 25,
    CPUID_EDX_SSE2 = 1 << 26,
    CPUID_EDX_SS = 1 << 27,
    CPUID_EDX_HTT = 1 << 28,
    CPUID_EDX_TM = 1 << 29,
    CPUID_EDX_RESERVED2 = 1 << 30,
    CPUID_EDX_PBE = 1 << 31,
} cpuid_edx_features_t;

/**
 * @brief EBX feature flags.
 * @enum cpuid_ebx_features_t
 *
 * These flags are returned in the EBX register after calling the CPUID instruction with
 * EAX=CPUID_EAX_EXTENDED_FEATURE_INFO and ECX=0.
 */
typedef enum
{
    CPUID_EBX_FSGSBASE = 1 << 0,
    CPUID_EBX_TSC_ADJUST = 1 << 1,
    CPUID_EBX_SGX = 1 << 2,
    CPUID_EBX_BMI1 = 1 << 3,
    CPUID_EBX_HLE = 1 << 4,
    CPUID_EBX_AVX2 = 1 << 5,
    CPUID_EBX_FDP_EXCPTN_ONLY = 1 << 6,
    CPUID_EBX_SMEP = 1 << 7,
    CPUID_EBX_BMI2 = 1 << 8,
    CPUID_EBX_ERMS = 1 << 9,
    CPUID_EBX_INVPCID = 1 << 10,
    CPUID_EBX_RTM = 1 << 11,
    CPUID_EBX_RDT_M = 1 << 12,
    CPUID_EBX_FPU_CS_DS_DEPR = 1 << 13,
    CPUID_EBX_MPX = 1 << 14,
    CPUID_EBX_RDT_A = 1 << 15,
    CPUID_EBX_AVX512F = 1 << 16,
    CPUID_EBX_AVX512DQ = 1 << 17,
    CPUID_EBX_RDSEED = 1 << 18,
    CPUID_EBX_ADX = 1 << 19,
    CPUID_EBX_SMAP = 1 << 20,
    CPUID_EBX_AVX512_IFMA = 1 << 21,
    CPUID_EBX_RESERVED1 = 1 << 22,
    CPUID_EBX_CLFLUSHOPT = 1 << 23,
    CPUID_EBX_CLWB = 1 << 24,
    CPUID_EBX_INTEL_PT = 1 << 25,
    CPUID_EBX_AVX512PF = 1 << 26,
    CPUID_EBX_AVX512ER = 1 << 27,
    CPUID_EBX_AVX512CD = 1 << 28,
    CPUID_EBX_SHA = 1 << 29,
    CPUID_EBX_AVX512BW = 1 << 30,
    CPUID_EBX_AVX512VL = 1 << 31,
} cpuid_ebx_features_t;

/**
 * @brief Output structure for CPUID instruction.
 * @struct cpuid_output_t
 */
typedef struct
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_output_t;

/**
 * @brief CPUID instruction.
 *
 * @param eax Input EAX value.
 * @param ecx Input ECX value.
 * @param out Output structure.
 */
static inline void cpuid(cpuid_input_eax_t eax, cpuid_input_ecx_t ecx, cpuid_output_t* out)
{
    ASM("cpuid" : "=a"(out->eax), "=b"(out->ebx), "=c"(out->ecx), "=d"(out->edx) : "a"(eax), "c"(ecx));
}

/**
 * @brief CPU feature information structure.
 * @struct cpuid_feature_info_t
 */
typedef struct
{
    uint32_t version;
    uint32_t brandClflushApicid;
    cpuid_ecx_features_t featuresEcx;
    cpuid_edx_features_t featuresEdx;
} cpuid_feature_info_t;

/**
 * @brief Wrapper to get CPU feature information.
 *
 * @param info Output pointer.
 */
static inline void cpuid_feature_info(cpuid_feature_info_t* info)
{
    cpuid_output_t out;
    cpuid(CPUID_EAX_FEATURE_INFO, CPUID_ECX_NONE, &out);
    info->version = out.eax;
    info->brandClflushApicid = out.ebx;
    info->featuresEcx = (cpuid_ecx_features_t)out.ecx;
    info->featuresEdx = (cpuid_edx_features_t)out.edx;
}

/**
 * @brief CPU extended feature information structure.
 * @struct cpuid_extended_feature_info_t
 */
typedef struct
{
    cpuid_ebx_features_t featuresEbx;
} cpuid_extended_feature_info_t;

/**
 * @brief Wrapper to get CPU extended feature information.
 *
 * @param info Output pointer.
 */
static inline void cpuid_extended_feature_info(cpuid_extended_feature_info_t* info)
{
    cpuid_output_t out;
    cpuid(CPUID_EAX_EXTENDED_FEATURE_INFO, CPUID_ECX_NONE, &out);
    info->featuresEbx = (cpuid_ebx_features_t)out.ebx;
}

/**
 * @brief Supported CPU instruction sets.
 * @enum cpuid_instruction_sets_t
 */
typedef enum
{
    CPUID_INSTRUCTION_SET_NONE = 0,
    CPUID_INSTRUCTION_SET_SSE = 1 << 0,
    CPUID_INSTRUCTION_SET_SSE2 = 1 << 1,
    CPUID_INSTRUCTION_SET_SSE3 = 1 << 2,
    CPUID_INSTRUCTION_SET_SSSE3 = 1 << 3,
    CPUID_INSTRUCTION_SET_SSE4_1 = 1 << 4,
    CPUID_INSTRUCTION_SET_SSE4_2 = 1 << 5,
    CPUID_INSTRUCTION_SET_AVX = 1 << 6,
    CPUID_INSTRUCTION_SET_AVX2 = 1 << 7,
    CPUID_INSTRUCTION_SET_AVX512 = 1 << 8,
} cpuid_instruction_sets_t;

/**
 * @brief Helper to detect supported instruction sets.
 *
 * @return A bitmask of supported instruction sets.
 */
static inline cpuid_instruction_sets_t cpuid_detect_instruction_sets(void)
{
    cpuid_instruction_sets_t sets = CPUID_INSTRUCTION_SET_NONE;
    cpuid_feature_info_t featureInfo;
    cpuid_feature_info(&featureInfo);
    if (featureInfo.featuresEdx & CPUID_EDX_SSE)
    {
        sets |= CPUID_INSTRUCTION_SET_SSE;
    }
    if (featureInfo.featuresEdx & CPUID_EDX_SSE2)
    {
        sets |= CPUID_INSTRUCTION_SET_SSE2;
    }
    if (featureInfo.featuresEcx & CPUID_ECX_SSE3)
    {
        sets |= CPUID_INSTRUCTION_SET_SSE3;
    }
    if (featureInfo.featuresEcx & CPUID_ECX_SSSE3)
    {
        sets |= CPUID_INSTRUCTION_SET_SSSE3;
    }
    if (featureInfo.featuresEcx & CPUID_ECX_SSE4_1)
    {
        sets |= CPUID_INSTRUCTION_SET_SSE4_1;
    }
    if (featureInfo.featuresEcx & CPUID_ECX_SSE4_2)
    {
        sets |= CPUID_INSTRUCTION_SET_SSE4_2;
    }
    if (featureInfo.featuresEcx & CPUID_ECX_AVX)
    {
        sets |= CPUID_INSTRUCTION_SET_AVX;
    }
    cpuid_extended_feature_info_t extFeatureInfo;
    cpuid_extended_feature_info(&extFeatureInfo);
    if (extFeatureInfo.featuresEbx & CPUID_EBX_AVX2)
    {
        sets |= CPUID_INSTRUCTION_SET_AVX2;
    }
    if (extFeatureInfo.featuresEbx & CPUID_EBX_AVX512F)
    {
        sets |= CPUID_INSTRUCTION_SET_AVX512;
    }
    return sets;
}

/** @} */

#endif // _SYS_CPUID_H
