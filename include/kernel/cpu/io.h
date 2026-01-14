#pragma once

#include <stdint.h>
#include <sys/defs.h>

/**
 * @brief I/O port operations and reservations
 * @defgroup kernel_cpu_io Port I/O
 * @ingroup kernel_cpu
 *
 * The CPU can communicate with certain hardware through I/O ports, these ports are accessed using special opcodes.
 *
 * ## Reserving I/O Ports
 *
 * To avoid conflicts between different subsystems or drivers trying to use the same I/O ports, we provide a simple
 * reservation mechanism. Before a range of I/O ports is used, it should be reserved using `io_reserve()`. Once the
 * ports are no longer needed, they should be released using `io_release()`.
 *
 * There is no strict enforcement of I/O port reservations at the hardware level, so we have no choice but to hope that
 * everyone is on their best behaviour.
 *
 * @{
 */

/**
 * @brief I/O port type
 */
typedef uint16_t port_t;

/**
 * @brief Maximum I/O port number
 */
#define IO_PORT_MAX UINT16_MAX

/**
 * @brief Find and reserve a range of I/O ports if available.
 *
 * @note The `minBase` and `maxBase` do NOT specify the exact range to reserve, but rather the minimum and maximum
 * values for the starting port of the range to reserve. For example, the minimum range would be `[minBase, minBase +
 * length)` and the maximum range would be `[maxBase, maxBase + length)`.
 *
 * @todo Find a way to store the owner of each reservation that does not use way to much memory, and isent super slow.
 *
 * @param out Output pointer for the first reserved port.
 * @param minBase The minimum base I/O port address.
 * @param maxBase The maximum base I/O port address.
 * @param alignment The alignment of the I/O ports to reserve.
 * @param length The amount of contiguous I/O ports to reserve.
 * @param owner A string identifying the owner of the reservation, for debugging purposes, can be `NULL`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EOVERFLOW`: The requested range overflows.
 * - `ENOSPC`: No suitable range of I/O ports available.
 */
uint64_t io_reserve(port_t* out, port_t minBase, port_t maxBase, uint64_t alignment, uint64_t length,
    const char* owner);

/**
 * @brief Release a previously reserved range of I/O ports.
 *
 * @param base The base I/O port address of the reserved range.
 * @param length The amount of contiguous I/O ports to release.
 */
void io_release(port_t base, uint64_t length);

/**
 * @brief Write an 8-bit value to an I/O port.
 *
 * @param port The I/O port to write to.
 * @param val The value to write.
 */
static inline void io_out8(port_t port, uint8_t val)
{
    ASM("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

/**
 * @brief Read an 8-bit value from an I/O port.
 *
 * @param port The I/O port to read from.
 * @return The value read from the port.
 */
static inline uint8_t io_in8(port_t port)
{
    uint8_t ret;
    ASM("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/**
 * @brief Write a 16-bit value to an I/O port.
 *
 * @param port The I/O port to write to.
 * @param val The value to write.
 */
static inline void io_out16(port_t port, uint16_t val)
{
    ASM("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

/**
 * @brief Read a 16-bit value from an I/O port.
 *
 * @param port The I/O port to read from.
 * @return The value read from the port.
 */
static inline uint16_t io_in16(port_t port)
{
    uint16_t ret;
    ASM("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/**
 * @brief Write a 32-bit value to an I/O port.
 *
 * @param port The I/O port to write to.
 * @param val The value to write.
 */
static inline uint32_t io_in32(port_t port)
{
    uint32_t ret;
    ASM("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/**
 * @brief Read a 32-bit value from an I/O port.
 *
 * @param port The I/O port to read from.
 * @return The value read from the port.
 */
static inline void io_out32(port_t port, uint32_t val)
{
    ASM("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

/** @} */
