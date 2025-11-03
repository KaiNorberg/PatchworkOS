#ifndef _SYS_ELF_H
#define _SYS_ELF_H 1

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Executable and linkable format definitions.
 * @defgroup libstd_sys_elf ELF file
 * @ingroup libstd
 *
 * The ELF (Executable and Linkable Format) is a commonly utailized file format for generic binary files, including
 * executables, object code, shared libraries, etc. and we utilize it in various parts of the operating system.
 *
 * Only 64-bit ELF is implemented/handled in PatchworkOS.
 *
 * For the sake of alignment with the ELF specification we use the same naming conventions, even if they are different
 * from the usual conventions used in PatchworkOS.
 *
 * @see [ELF Specification](https://gabi.xinuos.com/index.html) for more information.
 * @see [https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf] for the x86_64 ABI specification.
 *
 * @{
 */

/**
 * @brief ELF64 Unsigned program address
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint64_t Elf64_Addr;

/**
 * @brief ELF64 Unsigned file offset
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint64_t Elf64_Off;

/**
 * @brief ELF64 Unsigned medium integer
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint16_t Elf64_Half;

/**
 * @brief ELF64 Unsigned integer
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint32_t Elf64_Word;

/**
 * @brief ELF64 Signed integer
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef int32_t Elf64_Sword;

/**
 * @brief ELF64 Unsigned long integer
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint64_t Elf64_Xword;

/**
 * @brief ELF64 Signed long integer
 * @see https://gabi.xinuos.com/elf/01-intro.html#file-format
 */
typedef uint64_t Elf64_Sxword;

/**
 * @brief Indices for `e_ident[]`.
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    EI_MAG0 = 0,       ///< Index of magic number byte 0
    EI_MAG1 = 1,       ///< Index of magic number byte 1
    EI_MAG2 = 2,       ///< Index of magic number byte 2
    EI_MAG3 = 3,       ///< Index of magic number byte 3
    EI_CLASS = 4,      ///< Index of the file class byte
    EI_DATA = 5,       ///< Index of the data encoding byte
    EI_VERSION = 6,    ///< Index of the file version byte
    EI_OSABI = 7,      ///< Index of the OS/ABI identification byte
    EI_ABIVERSION = 8, ///< Index of the ABI version byte
    EI_PAD = 9,        ///< Index of the start of padding bytes
    EI_NIDENT = 16     ///< Total size of e_ident
} Elf64_Ident_Indexes;

/**
 * @brief ELF64 Header
 *
 * Stored in the beginning of an ELF file.
 *
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef struct
{
    unsigned char e_ident[EI_NIDENT]; ///< Identification bytes
    Elf64_Half e_type;                ///< Object file type
    Elf64_Half e_machine;             ///< The required architecture
    Elf64_Word e_version;             ///< Object file version
    Elf64_Addr e_entry;               ///< Entry point virtual address
    Elf64_Off e_phoff;      ///< Program header tables's file offset in bytes, or 0 if there are no program headers
    Elf64_Off e_shoff;      ///< Section header table's file offset in bytes, or 0 if there are no section headers
    Elf64_Word e_flags;     ///< Processor-specific flags
    Elf64_Half e_ehsize;    ///< Size of this header in bytes, should be `sizeof(Elf64_Ehdr)`
    Elf64_Half e_phentsize; ///< Size in bytes of one entry in the files program header table.
    Elf64_Half e_phnum;     ///< Number of entries in the program header table.
    Elf64_Half e_shentsize; ///< Size in bytes of one entry in the files section header table.
    /**
     * Number of entries in the section header table, or `0` if there are no section headers.
     *
     * If the number of sections is greater than or equal to `SHN_LORESERVE` (0xff00), this field contains `0` and the
     * actual number of section header table entires is contained in the `sh_size` field of section header index `0`.
     */
    Elf64_Half e_shnum;
    /**
     * Section header table index of the entry associated with the section name string table, or `SHN_UNDEF` if there
     * are no section names.
     *
     * If the section name string table section index is greater than or equal to `SHN_LORESERVE` (0xff00), this field
     * contains `SHN_XINDEX(0xffff)` and the actual section index is contained in the `sh_link` field of section header
     * index `0`.
     */
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

/**
 * @brief Expected magic values in `e_ident[EI_MAG0]` to `e_ident[EI_MAG3]`.
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    ELFMAG0 = 0x7f, ///< Expected value for `e_ident[EI_MAG0]`
    ELFMAG1 = 'E',  ///< Expected value for `e_ident[EI_MAG1]`
    ELFMAG2 = 'L',  ///< Expected value for `e_ident[EI_MAG2]`
    ELFMAG3 = 'F'   ///< Expected value for `e_ident[EI_MAG3]`
} Elf64_Magic;

/**
 * @brief File class values for `e_ident[EI_CLASS]`.
 *
 * Identifies whether the file is 32-bit or 64-bit.
 *
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    ELFCLASSNONE = 0, ///< Invalid class
    ELFCLASS32 = 1,   ///< 32-bit objects
    ELFCLASS64 = 2    ///< 64-bit objects, we always expect this value
} Elf64_Class;

/**
 * @brief Data encoding values for `e_ident[EI_DATA]`.
 *
 * Identifies the endianness of the file.
 *
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    ELFDATANONE = 0, ///< Invalid data encoding
    ELFDATALSB = 1,  ///< Little-endian encoding, we always expect this value
    ELFDATAMSB = 2   ///< Big-endian encoding
} Elf64_Data;

/**
 * @brief Version values for `e_ident[EI_VERSION]` and `e_version`.
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    EV_NONE = 0,   ///< Invalid version
    EV_CURRENT = 1 ///< Current version, we always expect this value
} Elf64_Version;

/**
 * @brief OS/ABI identification values for `e_ident[EI_OSABI]`.
 *
 * Defines the expected operating system or ABI for the file.
 *
 * Even if we are in fact not Linux or GNU, we still expect this value or `0` since we for the most part follow the same
 * conventions.
 *
 * We ignore the "ABI Version" field `e_ident[EI_ABIVERSION]` entirely.
 *
 * @see https://gabi.xinuos.com/elf/b-osabi.html
 */
typedef enum
{
    ELFOSABI_NONE = 0,      ///< No extensions or unspecified
    ELFOSABI_HPUX = 1,      ///< Hewlett-Packard HP-UX
    ELFOSABI_NETBSD = 2,    ///< NetBSD
    ELFOSABI_GNU = 3,       ///< GNU, we always expect this value
    ELFOSABI_LINUX = 3,     ///< Linux, alias for ELFOSABI_GNU
    ELFOSABI_SOLARIS = 6,   ///< Sun Solaris
    ELFOSABI_AIX = 7,       ///< IBM AIX
    ELFOSABI_IRIX = 8,      ///< SGI Irix
    ELFOSABI_FREEBSD = 9,   ///< FreeBSD
    ELFOSABI_TRU64 = 10,    ///< Compaq TRU64 UNIX
    ELFOSABI_MODESTO = 11,  ///< Novell Modesto
    ELFOSABI_OPENBSD = 12,  ///< Open BSD
    ELFOSABI_OPENVMS = 13,  ///< Open VMS
    ELFOSABI_NSK = 14,      ///< Hewlett-Packard Non-Stop Kernel
    ELFOSABI_AROS = 15,     ///< Amiga Research OS
    ELFOSABI_FENIXOS = 16,  ///< Fenix OS
    ELFOSABI_CLOUDABI = 17, ///< Nuxi CloudABI
    ELFOSABI_OPENVOS = 18   ///< Stratus Technologies OpenVOS
} Elf64_OsAbi;

/**
 * @brief Object file type values for `e_type`.
 * @see https://gabi.xinuos.com/elf/02-eheader.html
 */
typedef enum
{
    ET_NONE = 0, ///< No file type
    ET_REL = 1,  ///< Relocatable file
    ET_EXEC = 2, ///< Executable file
    ET_DYN = 3,  ///< Shared object file
    ET_CORE = 4  ///< Core file
} Elf64_Type;

/**
 * @brief Machine architecture values for `e_machine`.
 * @see https://gabi.xinuos.com/elf/a-emachine.html
 */
typedef enum
{
    EM_NONE = 0,              ///< No machine
    EM_M32 = 1,               ///< AT&T WE 32100
    EM_SPARC = 2,             ///< SPARC
    EM_386 = 3,               ///< Intel 80386
    EM_68K = 4,               ///< Motorola 68000
    EM_88K = 5,               ///< Motorola 88000
    EM_IAMCU = 6,             ///< Intel MCU
    EM_860 = 7,               ///< Intel 80860
    EM_MIPS = 8,              ///< MIPS I Architecture
    EM_S370 = 9,              ///< IBM System/370 Processor
    EM_MIPS_RS3_LE = 10,      ///< MIPS RS3000 Little-endian
    EM_PARISC = 15,           ///< Hewlett-Packard PA-RISC
    EM_VPP500 = 17,           ///< Fujitsu VPP500
    EM_SPARC32PLUS = 18,      ///< Enhanced instruction set SPARC
    EM_960 = 19,              ///< Intel 80960
    EM_PPC = 20,              ///< PowerPC
    EM_PPC64 = 21,            ///< 64-bit PowerPC
    EM_S390 = 22,             ///< IBM System/390 Processor
    EM_SPU = 23,              ///< IBM SPU/SPC
    EM_V800 = 36,             ///< NEC V800
    EM_FR20 = 37,             ///< Fujitsu FR20
    EM_RH32 = 38,             ///< TRW RH-32
    EM_RCE = 39,              ///< Motorola RCE
    EM_ARM = 40,              ///< ARM 32-bit architecture (AARCH32)
    EM_ALPHA = 41,            ///< Digital Alpha
    EM_SH = 42,               ///< Hitachi SH
    EM_SPARCV9 = 43,          ///< SPARC Version 9
    EM_TRICORE = 44,          ///< Siemens TriCore embedded processor
    EM_ARC = 45,              ///< Argonaut RISC Core, Argonaut Technologies Inc.
    EM_H8_300 = 46,           ///< Hitachi H8/300
    EM_H8_300H = 47,          ///< Hitachi H8/300H
    EM_H8S = 48,              ///< Hitachi H8S
    EM_H8_500 = 49,           ///< Hitachi H8/500
    EM_IA_64 = 50,            ///< Intel IA-64 processor architecture
    EM_MIPS_X = 51,           ///< Stanford MIPS-X
    EM_COLDFIRE = 52,         ///< Motorola ColdFire
    EM_68HC12 = 53,           ///< Motorola M68HC12
    EM_MMA = 54,              ///< Fujitsu MMA Multimedia Accelerator
    EM_PCP = 55,              ///< Siemens PCP
    EM_NCPU = 56,             ///< Sony nCPU embedded RISC processor
    EM_NDR1 = 57,             ///< Denso NDR1 microprocessor
    EM_STARCORE = 58,         ///< Motorola Star*Core processor
    EM_ME16 = 59,             ///< Toyota ME16 processor
    EM_ST100 = 60,            ///< STMicroelectronics ST100 processor
    EM_TINYJ = 61,            ///< Advanced Logic Corp. TinyJ embedded processor family
    EM_X86_64 = 62,           ///< AMD x86-64 architecture, we always expect this value
    EM_PDSP = 63,             ///< Sony DSP Processor
    EM_PDP10 = 64,            ///< Digital Equipment Corp. PDP-10
    EM_PDP11 = 65,            ///< Digital Equipment Corp. PDP-11
    EM_FX66 = 66,             ///< Siemens FX66 microcontroller
    EM_ST9PLUS = 67,          ///< STMicroelectronics ST9+ 8/16 bit microcontroller
    EM_ST7 = 68,              ///< STMicroelectronics ST7 8-bit microcontroller
    EM_68HC16 = 69,           ///< Motorola MC68HC16 Microcontroller
    EM_68HC11 = 70,           ///< Motorola MC68HC11 Microcontroller
    EM_68HC08 = 71,           ///< Motorola MC68HC08 Microcontroller
    EM_68HC05 = 72,           ///< Motorola MC68HC05 Microcontroller
    EM_SVX = 73,              ///< Silicon Graphics SVx
    EM_ST19 = 74,             ///< STMicroelectronics ST19 8-bit microcontroller
    EM_VAX = 75,              ///< Digital VAX
    EM_CRIS = 76,             ///< Axis Communications 32-bit embedded processor
    EM_JAVELIN = 77,          ///< Infineon Technologies 32-bit embedded processor
    EM_FIREPATH = 78,         ///< Element 14 64-bit DSP Processor
    EM_ZSP = 79,              ///< LSI Logic 16-bit DSP Processor
    EM_MMIX = 80,             ///< Donald Knuth’s educational 64-bit processor
    EM_HUANY = 81,            ///< Harvard University machine-independent object files
    EM_PRISM = 82,            ///< SiTera Prism
    EM_AVR = 83,              ///< Atmel AVR 8-bit microcontroller
    EM_FR30 = 84,             ///< Fujitsu FR30
    EM_D10V = 85,             ///< Mitsubishi D10V
    EM_D30V = 86,             ///< Mitsubishi D30V
    EM_V850 = 87,             ///< NEC v850
    EM_M32R = 88,             ///< Mitsubishi M32R
    EM_MN10300 = 89,          ///< Matsushita MN10300
    EM_MN10200 = 90,          ///< Matsushita MN10200
    EM_PJ = 91,               ///< picoJava
    EM_OPENRISC = 92,         ///< OpenRISC 32-bit embedded processor
    EM_ARC_COMPACT = 93,      ///< ARC International ARCompact processor (old spelling/synonym: EM_ARC_A5)
    EM_XTENSA = 94,           ///< Tensilica Xtensa Architecture
    EM_VIDEOCORE = 95,        ///< Alphamosaic VideoCore processor
    EM_TMM_GPP = 96,          ///< Thompson Multimedia General Purpose Processor
    EM_NS32K = 97,            ///< National Semiconductor 32000 series
    EM_TPC = 98,              ///< Tenor Network TPC processor
    EM_SNP1K = 99,            ///< Trebia SNP 1000 processor
    EM_ST200 = 100,           ///< STMicroelectronics (www.st.com) ST200 microcontroller
    EM_IP2K = 101,            ///< Ubicom IP2xxx microcontroller family
    EM_MAX = 102,             ///< MAX Processor
    EM_CR = 103,              ///< National Semiconductor CompactRISC microprocessor
    EM_F2MC16 = 104,          ///< Fujitsu F2MC16
    EM_MSP430 = 105,          ///< Texas Instruments embedded microcontroller msp430
    EM_BLACKFIN = 106,        ///< Analog Devices Blackfin (DSP) processor
    EM_SE_C33 = 107,          ///< S1C33 Family of Seiko Epson processors
    EM_SEP = 108,             ///< Sharp embedded microprocessor
    EM_ARCA = 109,            ///< Arca RISC Microprocessor
    EM_UNICORE = 110,         ///< Microprocessor series from PKU-Unity Ltd. and MPRC of Peking University
    EM_EXCESS = 111,          ///< eXcess: 16/32/64-bit configurable embedded CPU
    EM_DXP = 112,             ///< Icera Semiconductor Inc. Deep Execution Processor
    EM_ALTERA_NIOS2 = 113,    ///< Altera Nios II soft-core processor
    EM_CRX = 114,             ///< National Semiconductor CompactRISC CRX microprocessor
    EM_XGATE = 115,           ///< Motorola XGATE embedded processor
    EM_C166 = 116,            ///< Infineon C16x/XC16x processor
    EM_M16C = 117,            ///< Renesas M16C series microprocessors
    EM_DSPIC30F = 118,        ///< Microchip Technology dsPIC30F Digital Signal Controller
    EM_CE = 119,              ///< Freescale Communication Engine RISC core
    EM_M32C = 120,            ///< Renesas M32C series microprocessors
    EM_TSK3000 = 131,         ///< Altium TSK3000 core
    EM_RS08 = 132,            ///< Freescale RS08 embedded processor
    EM_SHARC = 133,           ///< Analog Devices SHARC family of 32-bit DSP processors
    EM_ECOG2 = 134,           ///< Cyan Technology eCOG2 microprocessor
    EM_SCORE7 = 135,          ///< Sunplus S+core7 RISC processor
    EM_DSP24 = 136,           ///< New Japan Radio (NJR) 24-bit DSP Processor
    EM_VIDEOCORE3 = 137,      ///< Broadcom VideoCore III processor
    EM_LATTICEMICO32 = 138,   ///< RISC processor for Lattice FPGA architecture
    EM_SE_C17 = 139,          ///< Seiko Epson C17 family
    EM_TI_C6000 = 140,        ///< The Texas Instruments TMS320C6000 DSP family
    EM_TI_C2000 = 141,        ///< The Texas Instruments TMS320C2000 DSP family
    EM_TI_C5500 = 142,        ///< The Texas Instruments TMS320C55x DSP family
    EM_TI_ARP32 = 143,        ///< Texas Instruments Application Specific RISC Processor, 32bit fetch
    EM_TI_PRU = 144,          ///< Texas Instruments Programmable Realtime Unit
    EM_MMDSP_PLUS = 160,      ///< STMicroelectronics 64bit VLIW Data Signal Processor
    EM_CYPRESS_M8C = 161,     ///< Cypress M8C microprocessor
    EM_R32C = 162,            ///< Renesas R32C series microprocessors
    EM_TRIMEDIA = 163,        ///< NXP Semiconductors TriMedia architecture family
    EM_QDSP6 = 164,           ///< QUALCOMM DSP6 Processor
    EM_8051 = 165,            ///< Intel 8051 and variants
    EM_STXP7X = 166,          ///< STMicroelectronics STxP7x family of configurable and extensible RISC processors
    EM_NDS32 = 167,           ///< Andes Technology compact code size embedded RISC processor family
    EM_ECOG1 = 168,           ///< Cyan Technology eCOG1X family
    EM_ECOG1X = 168,          ///< Cyan Technology eCOG1X family
    EM_MAXQ30 = 169,          ///< Dallas Semiconductor MAXQ30 Core Micro-controllers
    EM_XIMO16 = 170,          ///< New Japan Radio (NJR) 16-bit DSP Processor
    EM_MANIK = 171,           ///< M2000 Reconfigurable RISC Microprocessor
    EM_CRAYNV2 = 172,         ///< Cray Inc. NV2 vector architecture
    EM_RX = 173,              ///< Renesas RX family
    EM_METAG = 174,           ///< Imagination Technologies META processor architecture
    EM_MCST_ELBRUS = 175,     ///< MCST Elbrus general purpose hardware architecture
    EM_ECOG16 = 176,          ///< Cyan Technology eCOG16 family
    EM_CR16 = 177,            ///< National Semiconductor CompactRISC CR16 16-bit microprocessor
    EM_ETPU = 178,            ///< Freescale Extended Time Processing Unit
    EM_SLE9X = 179,           ///< Infineon Technologies SLE9X core
    EM_L10M = 180,            ///< Intel L10M
    EM_K10M = 181,            ///< Intel K10M
    EM_AARCH64 = 183,         ///< ARM 64-bit architecture (AARCH64)
    EM_AVR32 = 185,           ///< Atmel Corporation 32-bit microprocessor family
    EM_STM8 = 186,            ///< STMicroeletronics STM8 8-bit microcontroller
    EM_TILE64 = 187,          ///< Tilera TILE64 multicore architecture family
    EM_TILEPRO = 188,         ///< Tilera TILEPro multicore architecture family
    EM_MICROBLAZE = 189,      ///< Xilinx MicroBlaze 32-bit RISC soft processor core
    EM_CUDA = 190,            ///< NVIDIA CUDA architecture
    EM_TILEGX = 191,          ///< Tilera TILE-Gx multicore architecture family
    EM_CLOUDSHIELD = 192,     ///< CloudShield architecture family
    EM_COREA_1ST = 193,       ///< KIPO-KAIST Core-A 1st generation processor family
    EM_COREA_2ND = 194,       ///< KIPO-KAIST Core-A 2nd generation processor family
    EM_ARC_COMPACT2 = 195,    ///< Synopsys ARCompact V2
    EM_OPEN8 = 196,           ///< Open8 8-bit RISC soft processor core
    EM_RL78 = 197,            ///< Renesas RL78 family
    EM_VIDEOCORE5 = 198,      ///< Broadcom VideoCore V processor
    EM_78KOR = 199,           ///< Renesas 78KOR family
    EM_56800EX = 200,         ///< Freescale 56800EX Digital Signal Controller (DSC)
    EM_BA1 = 201,             ///< Beyond BA1 CPU architecture
    EM_BA2 = 202,             ///< Beyond BA2 CPU architecture
    EM_XCORE = 203,           ///< XMOS xCORE processor family
    EM_MCHP_PIC = 204,        ///< Microchip 8-bit PIC(r) family
    EM_INTEL205 = 205,        ///< Reserved by Intel
    EM_INTEL206 = 206,        ///< Reserved by Intel
    EM_INTEL207 = 207,        ///< Reserved by Intel
    EM_INTEL208 = 208,        ///< Reserved by Intel
    EM_INTEL209 = 209,        ///< Reserved by Intel
    EM_KM32 = 210,            ///< KM211 KM32 32-bit processor
    EM_KMX32 = 211,           ///< KM211 KMX32 32-bit processor
    EM_KMX16 = 212,           ///< KM211 KMX16 16-bit processor
    EM_KMX8 = 213,            ///< KM211 KMX8 8-bit processor
    EM_KVARC = 214,           ///< KM211 KVARC processor
    EM_CDP = 215,             ///< Paneve CDP architecture family
    EM_COGE = 216,            ///< Cognitive Smart Memory Processor
    EM_COOL = 217,            ///< Bluechip Systems CoolEngine
    EM_NORC = 218,            ///< Nanoradio Optimized RISC
    EM_CSR_KALIMBA = 219,     ///< CSR Kalimba architecture family
    EM_Z80 = 220,             ///< Zilog Z80
    EM_VISIUM = 221,          ///< Controls and Data Services VISIUMcore processor
    EM_FT32 = 222,            ///< FTDI Chip FT32 high performance 32-bit RISC architecture
    EM_MOXIE = 223,           ///< Moxie processor family
    EM_AMDGPU = 224,          ///< AMD GPU architecture
    EM_RISCV = 243,           ///< RISC-V
    EM_LANAI = 244,           ///< Lanai processor
    EM_CEVA = 245,            ///< CEVA Processor Architecture Family
    EM_CEVA_X2 = 246,         ///< CEVA X2 Processor Family
    EM_BPF = 247,             ///< Linux BPF – in-kernel virtual machine
    EM_GRAPHCORE_IPU = 248,   ///< Graphcore Intelligent Processing Unit
    EM_IMG1 = 249,            ///< Imagination Technologies
    EM_NFP = 250,             ///< Netronome Flow Processor (NFP)
    EM_VE = 251,              ///< NEC Vector Engine
    EM_CSKY = 252,            ///< C-SKY processor family
    EM_ARC_COMPACT3_64 = 253, ///< Synopsys ARCv2.3 64-bit
    EM_MCS6502 = 254,         ///< MOS Technology MCS 6502 processor
    EM_ARC_COMPACT3 = 255,    ///< Synopsys ARCv2.3 32-bit
    EM_KVX = 256,             ///< Kalray VLIW core of the MPPA processor family
    EM_65816 = 257,           ///< WDC 65816/65C816
    EM_LOONGARCH = 258,       ///< Loongson Loongarch
    EM_KF32 = 259,            ///< ChipON KungFu32
    EM_U16_U8CORE = 260,      ///< LAPIS nX-U16/U8
    EM_TACHYUM = 261,         ///< Reserved for Tachyum processor
    EM_56800EF = 262,         ///< NXP 56800EF Digital Signal Controller (DSC)
    EM_SBF = 263,             ///< Solana Bytecode Format
    EM_AIENGINE = 264,        ///< AMD/Xilinx AIEngine architecture
    EM_SIMA_MLA = 265,        ///< SiMa MLA
    EM_BANG = 266,            ///< Cambricon BANG
    EM_LOONGGPU = 267,        ///< Loongson LoongGPU
    EM_SW64 = 268,            ///< Wuxi Institute of Advanced Technology SW64
    EM_AIECTRLCODE = 269,     ///< AMD/Xilinx AIEngine ctrlcode
} Elf64_Machine;

/**
 * @brief Special section indexes.
 * @see https://gabi.xinuos.com/elf/03-sheader.html
 */
typedef enum
{
    SHN_UNDEF = 0,          ///< Undefined section
    SHN_LORESERVE = 0xff00, ///< Start of reserved indexes
    SHN_LOPROC = 0xff00,    ///< Start of processor-specific indexes
    SHN_HIPROC = 0xff1f,    ///< End of processor-specific indexes
    SHN_LOOS = 0xff20,      ///< Start of OS-specific indexes
    SHN_HIOS = 0xff3f,      ///< End of OS-specific indexes
    SHN_ABS = 0xfff1,       ///< Specifies absolute values for the corresponding reference
    SHN_COMMON = 0xfff2,    ///< Symbols defined relative to this section are common symbols
    SHN_XINDEX = 0xffff,    ///< Indicates that the actual index is too large to fit and is stored elsewhere
    SHN_HIRESERVE = 0xffff  ///< End of reserved indexes
} Elf64_Shn_Indexes;

/**
 * @brief ELF64 Section Header
 * @see https://gabi.xinuos.com/elf/03-sheader.html
 *
 * Stored in the section header table, which is located at the file offset `e_shoff` and contains `e_shnum` entries
 * where each entry is `e_shentsize` bytes long.
 *
 * Each section header describes a section in the ELF file, for example the `.text` or `.data` sections.
 */
typedef struct
{
    Elf64_Word sh_name;       ///< Index of the section name in the string table
    Elf64_Word sh_type;       ///< Section type
    Elf64_Xword sh_flags;     ///< Section flags
    Elf64_Addr sh_addr;       ///< If the section will appear in memory, this will be its virtual address, otherwise `0`
    Elf64_Off sh_offset;      ///< Section's file offset in bytes
    Elf64_Xword sh_size;      ///< Section size in bytes
    Elf64_Word sh_link;       ///< Depends on section type, for symbol tables this is the section header index of the
                              ///< associated string table
    Elf64_Word sh_info;       ///< Depends on section type
    Elf64_Xword sh_addralign; ///< Section byte alignment requirement
    Elf64_Xword sh_entsize;   ///< If the section holds a table of fixed-size entries, this is the size of each entry,
                              /// otherwise `0`
} Elf64_Shdr;

/**
 * @brief Section type values for `sh_type`.
 * @see https://gabi.xinuos.com/elf/03-sheader.html
 */
typedef enum
{
    SHT_NULL = 0,        ///< Does not have an associated section
    SHT_PROGBITS = 1,    ///< Contains information defined by the program
    SHT_SYMTAB = 2,      ///< Contains a symbol table, only 1 per file
    SHT_STRTAB = 3,      ///< Contains a string table
    SHT_RELA = 4,        ///< Contains relocation entries with explicit addends
    SHT_HASH = 5,        ///< Contains a symbol hash table, only 1 per file
    SHT_DYNAMIC = 6,     ///< Contains dynamic linking information, only 1 per file
    SHT_NOTE = 7,        ///< Contains unspecified auxiliary information
    SHT_NOBITS = 8,      ///< Acts like `SHT_PROGBITS` but does not occupy any space in the file
    SHT_REL = 9,         ///< Contains relocation entries without explicit addends
    SHT_SHLIB = 10,      ///< Reserved, has unspecified semantics
    SHT_DYNSYM = 11,     ///< Acts like `SHT_SYMTAB` but holds a minimal set of dynamic linking symbols, only 1 per file
    SHT_INIT_ARRAY = 14, ///< Contains an array of pointers to initialization functions
    SHT_FINI_ARRAY = 15, ///< Contains an array of pointers to termination functions
    SHT_PREINIT_ARRAY = 16,  ///< Contains an array of pointers to pre-initialization functions
    SHT_GROUP = 17,          ///< Contains a section group, can only appear in relocatable files
    SHT_SYMTAB_SHNDX = 18,   ///< Contains extended section indexes for a symbol table, used with `SHN_XINDEX`
    SHT_RELR = 19,           ///< Contains relocation entries for relative relocations without explicit addends
    SHT_LOOS = 0x60000000,   ///< Start of OS-specific section types
    SHT_HIOS = 0x6fffffff,   ///< End of OS-specific section types
    SHT_LOPROC = 0x70000000, ///< Start of processor-specific section types
    SHT_HIPROC = 0x7fffffff, ///< End of processor-specific section types
    SHT_LOUSER = 0x80000000, ///< Start of application-specific section types
    SHT_HIUSER = 0xffffffff, ///< End of application-specific section types
} Elf64_Section_Types;

/**
 * @brief Section flag values for `sh_flags`.
 * @see https://gabi.xinuos.com/elf/03-sheader.html
 */
typedef enum
{
    SHF_WRITE = 0x1,              ///< Section should be writable when loaded to memory
    SHF_ALLOC = 0x2,              ///< Section should be loaded to memory
    SHF_EXECINSTR = 0x4,          ///< Section should be executable when loaded to memory
    SHF_MERGE = 0x10,             ///< Section may be merged to eliminate duplication
    SHF_STRINGS = 0x20,           ///< Section contains null-terminated strings, `sh_entsize` contains the char size
    SHF_INFO_LINK = 0x40,         ///< `sh_info` contains a section header table index
    SHF_LINK_ORDER = 0x80,        ///< Preserve section ordering when linking
    SHF_OS_NONCONFORMING = 0x100, ///< Section requires special OS-specific processing
    SHF_GROUP = 0x200,            ///< Is part of a section group
    SHF_TLS = 0x400,              ///< Section holds thread-local storage
    SHF_COMPRESSED = 0x800,       ///< Section holds compressed data
    SHF_MASKOS = 0x0ff00000,      ///< All bits in this mask are reserved for OS-specific semantics
    SHF_MASKPROC = 0xf0000000,    ///< All bits in this mask are reserved for processor-specific semantics
} Elf64_Section_Flags;

/**
 * @brief ELF64 Symbol Table Entry
 * @see https://gabi.xinuos.com/elf/05-symtab.html
 *
 * Stored in sections of type `SHT_SYMTAB` or `SHT_DYNSYM`.
 */
typedef struct
{
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

/**
 * @brief Extract the binding from `st_info`.
 *
 * @param i The `st_info` value
 * @return The binding value
 */
#define ELF64_ST_BIND(i) ((i) >> 4)

/**
 * @brief Symbol binding values stored in `st_info`.
 * @see https://gabi.xinuos.com/elf/05-symtab.html
 */
typedef enum
{
    STB_LOCAL = 0,   ///< Local symbol, not visible outside the object file
    STB_GLOBAL = 1,  ///< Global symbol, visible to all object files being combined
    STB_WEAK = 2,    ///< Weak symbol, like global but with lower precedence
    STB_LOOS = 10,   ///< Start of OS-specific symbol bindings
    STB_HIOS = 12,   ///< End of OS-specific symbol bindings
    STB_LOPROC = 13, ///< Start of processor-specific symbol bindings
    STB_HIPROC = 15  ///< End of processor-specific symbol bindings
} Elf64_Symbol_Binding;

/**
 * @brief Extract the type from `st_info`.
 *
 * @param i The `st_info` value
 * @return The type value
 */
#define ELF64_ST_TYPE(i) ((i) & 0xf)

/**
 * @brief Symbol type values stored in `st_info`.
 * @see https://gabi.xinuos.com/elf/05-symtab.html
 */
typedef enum
{
    STT_NOTYPE = 0,  ///< Symbol type is unspecified
    STT_OBJECT = 1,  ///< Symbol is a data object
    STT_FUNC = 2,    ///< Symbol is a code object
    STT_SECTION = 3, ///< Symbol associated with a section
    STT_FILE = 4,    ///< Symbol's name is the name of a source file
    STT_LOOS = 10,   ///< Start of OS-specific symbol types
    STT_HIOS = 12,   ///< End of OS-specific symbol types
    STT_LOPROC = 13, ///< Start of processor-specific symbol types
    STT_HIPROC = 15  ///< End of processor-specific symbol types
} Elf64_Symbol_Type;

/**
 * @brief Create an `st_info` value from binding and type.
 *
 * @see https://gabi.xinuos.com/elf/05-symtab.html
 *
 * @param b The binding value
 * @param t The type value
 * @return The combined `st_info` value
 */
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

/**
 * @brief ELF64 Rel Entry without addend
 * @see https://gabi.xinuos.com/elf/06-reloc.html
 */
typedef struct
{
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

/**
 * @brief ELF64 Rela Entry with addend
 * @see https://gabi.xinuos.com/elf/06-reloc.html
 */
typedef struct
{
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

/**
 * @brief Extract the symbol index from `r_info`.
 *
 * @see https://gabi.xinuos.com/elf/06-reloc.html
 *
 * @param i The `r_info` value
 * @return The symbol index
 */
#define ELF64_R_SYM(i) ((i) >> 32)

/**
 * @brief Extract the type from `r_info`.
 *
 * @see https://gabi.xinuos.com/elf/06-reloc.html
 *
 * @param i The `r_info` value
 * @return The type value
 */
#define ELF64_R_TYPE(i) ((i) & 0xffffffffL)

/**
 * @brief Relocation type values for `r_info`.
 *
 * The associated comments describe the calculation performed for each relocation type where:
 * - A = The addend used to compute the value of the relocatable field.
 * - B = The base address at which the object is loaded into memory
 * - G = The offset into the Global Offset Table
 * - GOT = The address of the Global Offset Table
 * - L = The address of the procedure linkage table entry for the symbol
 * - P = The place (section offset or address) of the storage unit being relocated
 * - S = value of the symbol in the relocation entry
 * - Z = The size of the symbol
 *
 * Additionally the size of the relocated field is indicated (word8, word16, word32, word64).
 *
 * Most of these are not used.
 *
 * @see https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf table 4.10
 */
typedef enum
{
    R_X86_64_NONE = 0,             ///< none none
    R_X86_64_64 = 1,               ///< word64 S + A
    R_X86_64_PC32 = 2,             ///< word32 S + A - P
    R_X86_64_GOT32 = 3,            ///< word32 G + A
    R_X86_64_PLT32 = 4,            ///< word32 L + A - P
    R_X86_64_COPY = 5,             ///< none none
    R_X86_64_GLOB_DAT = 6,         ///< word64 S
    R_X86_64_JUMP_SLOT = 7,        ///< word64 S
    R_X86_64_RELATIVE = 8,         ///< word64 B + A
    R_X86_64_GOTPCREL = 9,         ///< word32 G + GOT + A - P
    R_X86_64_32 = 10,              ///<  word32 S + A
    R_X86_64_32S = 11,             ///<  word32 S + A
    R_X86_64_16 = 12,              ///<  word16 S + A
    R_X86_64_PC16 = 13,            ///<  word16 S + A - P
    R_X86_64_8 = 14,               ///<  word8 S + A
    R_X86_64_PC8 = 15,             ///<  word8 S + A - P
    R_X86_64_DTPMOD64 = 16,        ///<  word64
    R_X86_64_DTPOFF64 = 17,        ///<  word64
    R_X86_64_TPOFF64 = 18,         ///<  word64
    R_X86_64_TLSGD = 19,           ///<  word32
    R_X86_64_TLSLD = 20,           ///<  word32
    R_X86_64_DTPOFF32 = 21,        ///<  word32
    R_X86_64_GOTTPOFF = 22,        ///<  word32
    R_X86_64_TPOFF32 = 23,         ///<  word32
    R_X86_64_PC64 = 24,            ///<  word64 S + A - P
    R_X86_64_GOTOFF64 = 25,        ///<  word64 S + A - GOT
    R_X86_64_GOTPC32 = 26,         ///<  word32 GOT + A - P
    R_X86_64_SIZE32 = 32,          ///<  word32 Z + A
    R_X86_64_SIZE64 = 33,          ///<  word64 Z + A
    R_X86_64_GOTPC32_TLSDESC = 34, ///<  word32
    R_X86_64_TLSDESC_CALL = 35,    ///<  none
    R_X86_64_TLSDESC = 36,         ///<  word64×2
    R_X86_64_IRELATIVE = 37,       ///<  word64 indirect (B + A)
} Elf64_Relocation_Types_x86_64;

/**
 * @brief Create an `r_info` value from symbol index and type.
 *
 * @see https://gabi.xinuos.com/elf/06-reloc.html
 *
 * @param s The symbol index
 * @param t The type value
 * @return The combined `r_info` value
 */
#define ELF64_R_INFO(s, t) (((s) << 32) + ((t) & 0xffffffffL))

/**
 * @brief ELF64 Program Header
 *
 * Stored in the program header table, which is located at the file offset `e_phoff` and contains `e_phnum` entries
 * where each entry is `e_phentsize` bytes long.
 *
 * Each program header describes a segment in the ELF file, which is used during program loading.
 *
 * @see https://gabi.xinuos.com/elf/07-pheader.html
 */
typedef struct
{
    Elf64_Word p_type;    ///< Segment type
    Elf64_Word p_flags;   ///< Segment flags
    Elf64_Off p_offset;   ///< Segment file offset in bytes
    Elf64_Addr p_vaddr;   ///< Target virtual address in memory
    Elf64_Addr p_paddr;   ///< Target physical address, ignored on systems without physical addressing
    Elf64_Xword p_filesz; ///< Size of segment in file in bytes
    Elf64_Xword p_memsz;  ///< Size of segment in memory in bytes
    Elf64_Xword p_align;  ///< Segment alignment requirement
} Elf64_Phdr;

/**
 * @brief Segment type values for `p_type`.
 * @see https://gabi.xinuos.com/elf/07-pheader.html
 */
typedef enum
{
    PT_NULL = 0,            ///< Unused segment
    PT_LOAD = 1,            ///< Loadable segment
    PT_DYNAMIC = 2,         ///< Dynamic linking information
    PT_INTERP = 3,          ///< Program interpreter path name
    PT_NOTE = 4,            ///< Auxiliary information
    PT_SHLIB = 5,           ///< Reserved, has unspecified semantics
    PT_PHDR = 6,            ///< Location and size of program header table
    PT_TLS = 7,             ///< Thread-local storage template
    PT_LOOS = 0x60000000,   ///< Start of OS-specific segment types
    PT_HIOS = 0x6fffffff,   ///< End of OS-specific segment types
    PT_LOPROC = 0x70000000, ///< Start of processor-specific segment types
    PT_HIPROC = 0x7fffffff  ///< End of processor-specific segment types
} Elf64_Program_Types;

/**
 * @brief Segment flag values for `p_flags`.
 * @see https://gabi.xinuos.com/elf/07-pheader.html
 *
 * A segment is allowed to be readable even if the `PF_R` flag is not set.
 */
typedef enum
{
    PF_X = 0x1,              ///< Executable segment
    PF_W = 0x2,              ///< Writable segment
    PF_R = 0x4,              ///< Readable segment
    PF_MASKOS = 0x0ff00000,  ///< All bits in this mask are reserved for OS-specific semantics
    PF_MASKPROC = 0xf0000000 ///< All bits in this mask are reserved for processor-specific semantics
} Elf64_Program_Flags;

/**
 * @brief ELF File Helper structure
 * @struct Elf64_File
 */
typedef struct
{
    Elf64_Ehdr* header; ///< The data in the file, pointed to the start of the ELF header
    uint64_t size;      ///< The size of the file in bytes
    Elf64_Shdr* symtab; ///< The symbol table section, or `NULL` if not found
    Elf64_Shdr* dynsym; ///< The dynamic symbol table section, or `NULL` if not found
} Elf64_File;

/**
 * @brief Get the program header at the given index from an ELF file
 *
 * @param elf The ELF file
 * @param index The program header index
 * @return Pointer to the program header
 */
#define ELF64_GET_PHDR(elf, index) \
    ((Elf64_Phdr*)((uint8_t*)(elf)->header + (elf)->header->e_phoff + ((index) * (elf)->header->e_phentsize)))

/**
 * @brief Get the section header at the given index from an ELF file
 *
 * @param elf The ELF file
 * @param index The section header index
 * @return Pointer to the section header
 */
#define ELF64_GET_SHDR(elf, index) \
    ((Elf64_Shdr*)((uint8_t*)(elf)->header + (elf)->header->e_shoff + ((index) * (elf)->header->e_shentsize)))
/**
 * @brief Get a pointer to a location in the ELF file at the given offset
 *
 * @param elf The ELF file
 * @param offset The offset in bytes
 * @return Pointer to the location in the ELF file
 */
#define ELF64_AT_OFFSET(elf, offset) ((void*)((uint8_t*)(elf)->header + (offset)))

/**
 * @brief Validate a files content and initalize a `ELF64_File` structure using it
 *
 * The idea behind this function is to verify every aspect of a ELF file such that other functions acting on the ELF64
 * file do not need to perform any validation.
 *
 * The reason this does not read from a file is such that it will be generic and usable in user space, in the kernel and
 * the bootloader.
 *
 * Having to load the entire file might seem wasteful, but its actually very important in order to avoid a situation
 * where we validate the file, another process modifies it, and then we read actual data later on causing a TOCTOU
 * vulnerability.
 *
 * @param elf Pointer to the structure to initialize
 * @param data Pointer to the ELF file data in memory, caller retains ownership
 * @param size Size of the ELF file data in bytes
 * @return On success, `0`. On failure, a non-zero error code. Check the implementation. Does not use `ERR` or `errno`.
 */
uint64_t elf64_validate(Elf64_File* elf, void* data, uint64_t size);

/**
 * @brief Get the loadable virtual memory bounds of an ELF file
 *
 * @param elf The ELF file
 * @param minAddr Output pointer to store the minimum loadable virtual address
 * @param maxAddr Output pointer to store the maximum loadable virtual address
 */
void elf64_get_loadable_bounds(const Elf64_File* elf, Elf64_Addr* minAddr, Elf64_Addr* maxAddr);

/**
 * @brief Load all loadable segments of an ELF file into memory
 *
 * Each segment has virtual addresses specified in `p_vaddr` which is where the segment is intended to be loaded in
 * memory. But we may not want to load it directly to that address, we might have a buffer where we wish to place the
 * segments instead. Either way, we must still place the segments at the correct offsets relative to each other. Leading
 * to the slightly unintuitive parameters of this function.
 *
 * The final address where a segment is loaded is calculated as `base + (p_vaddr - offset)`, meaning that if you wish to
 * load a file directly to its intended virtual addresses, you would do:
 * ```c
 * elf64_load_segments(elf, (void*)0x0, 0x0);
 * ```
 * If you wanted to load the contents to a buffer located at `buffer` which could later be mapped to the intended
 * virtual addresses or if you wanted to load relocatable code, you would do:
 * ```c
 * Elf64_Addr minAddr, maxAddr;
 * elf64_get_loadable_bounds(elf, &minAddr, &maxAddr);
 * elf64_load_segments(elf, buffer, minAddr);
 * ```
 *
 * @note This function does not allocate memory, it assumes that the caller has already allocated enough memory at `base
 * + (p_vaddr - offset)` for each segment.
 *
 * @param elf The ELF file
 * @param base The base address to load the segments into
 * @param offset The offset in bytes to subtract from each segment's virtual address when loading
 */
void elf64_load_segments(const Elf64_File* elf, Elf64_Addr base, Elf64_Off offset);

/**
 * @brief Perform relocations on an ELF file loaded into memory
 *
 * This function will process all relocation sections in the ELF file and apply the relocations to the loaded segments
 * in memory, including resolving symbol addresses using the provided callback as necessary.
 *
 * Relocations are necessary when a ELF file contains references to symbols whose addresses are not known at compile
 * time, for example the ELF file might be a shared library or kernel module.
 *
 * Check `elf64_load_segments()` for an explanation of the `base` and `offset` parameters.
 *
 * The `resolve_symbol` callback is used to resolve symbol names to addresses, this will be utilized for relocations of
 * undefined symbols. Should return `0` if the symbol could not be resolved.
 *
 * TOOD: Implement more relocation types as needed.
 *
 * @param elf The ELF file
 * @param base The base address where the segments are loaded in memory
 * @param offset The offset in bytes that was subtracted from each segment's virtual address when loading
 * @param resolve_symbol Callback function to resolve symbol names to addresses
 * @param private Private data pointer passed to the `resolve_symbol` callback
 * @return On success, `0`. On failure, `ERR`.
 */
uint64_t elf64_relocate(const Elf64_File* elf, Elf64_Addr base, Elf64_Off offset,
    Elf64_Addr (*resolve_symbol)(const char* name, void* private), void* private);

/**
 * @brief Get a string from the string table section at the given offset
 *
 * @param elf The ELF file
 * @param strTabIndex The index of the string table section to use
 * @param offset The offset in bytes into the string table
 * @return Pointer to the string in the ELF file or `NULL` if not found
 */
const char* elf64_get_string(const Elf64_File* elf, Elf64_Xword strTabIndex, Elf64_Off offset);

/**
 * @brief Get a section by its name
 *
 * @param elf The ELF file
 * @param name The name of the section to find
 * @return Pointer to the section header or `NULL` if not found
 */
Elf64_Shdr* elf64_get_section_by_name(const Elf64_File* elf, const char* name);

/**
 * @brief Get the name of a section
 *
 * @param elf The ELF file
 * @param section The section to get the name of
 * @return Pointer to the section name string or `NULL` if not found
 */
const char* elf64_get_section_name(const Elf64_File* elf, const Elf64_Shdr* section);

/**
 * @brief Get a symbol by its index from the symbol table
 *
 * @param elf The ELF file
 * @param symbolIndex The index of the symbol to get
 * @return Pointer to the symbol or `NULL` if not found
 */
Elf64_Sym* elf64_get_symbol_by_index(const Elf64_File* elf, Elf64_Xword symbolIndex);

/**
 * @brief Get the name of a symbol
 *
 * @param elf The ELF file
 * @param symbol The symbol to get the name of
 * @return Pointer to the symbol name string or `NULL` if not found
 */
const char* elf64_get_symbol_name(const Elf64_File* elf, const Elf64_Sym* symbol);

/**
 * @brief Get a dynamic symbol by its index from the dynamic symbol table
 *
 * Dynamic symbols are, for example, found in `.rela.*` sections used for dynamic linking.
 *
 * @param elf The ELF file
 * @param symbolIndex The index of the dynamic symbol to get
 * @return Pointer to the dynamic symbol or `NULL` if not found
 */
Elf64_Sym* elf64_get_dynamic_symbol_by_index(const Elf64_File* elf, Elf64_Xword symbolIndex);

/**
 * @brief Get the name of a dynamic symbol
 *
 * @param elf The ELF file
 * @param symbol The dynamic symbol to get the name of
 * @return Pointer to the dynamic symbol name string or `NULL` if not found
 */
const char* elf64_get_dynamic_symbol_name(const Elf64_File* elf, const Elf64_Sym* symbol);

/** @} */

#endif
