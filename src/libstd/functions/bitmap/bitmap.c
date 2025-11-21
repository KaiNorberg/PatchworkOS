#include <sys/bitmap.h>

void bitmap_init(bitmap_t* map, void* buffer, uint64_t length)
{
    map->firstZeroIdx = 0;
    map->length = length;
    map->buffer = (uint64_t*)buffer;
}

bool bitmap_is_set(bitmap_t* map, uint64_t idx)
{
    if (idx >= map->length)
    {
        return false;
    }

    uint64_t qwordIdx = idx / 64;
    uint64_t bitInQword = idx % 64;
    return (map->buffer[qwordIdx] & (1ULL << bitInQword));
}

void bitmap_set(bitmap_t* map, uint64_t index)
{
    if (index >= map->length)
    {
        return;
    }

    uint64_t qwordIdx = index / 64;
    uint64_t bitInQword = index % 64;
    map->buffer[qwordIdx] |= (1ULL << bitInQword);
}

void bitmap_set_range(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
    {
        return;
    }

    uint64_t firstQwordIdx = low / 64;
    uint64_t firstBitInQword = low % 64;
    uint64_t lastQwordIdx = (high - 1) / 64;
    uint64_t lastBitInQword = (high - 1) % 64;

    if (firstQwordIdx == lastQwordIdx)
    {
        uint64_t mask = (~0ULL << firstBitInQword) & (~0ULL >> (63 - lastBitInQword));
        map->buffer[firstQwordIdx] |= mask;
        return;
    }

    map->buffer[firstQwordIdx] |= (~0ULL << firstBitInQword);

    for (uint64_t i = firstQwordIdx + 1; i < lastQwordIdx; i++)
    {
        map->buffer[i] = ~0ULL;
    }

    map->buffer[lastQwordIdx] |= (~0ULL >> (63 - lastBitInQword));
}

void bitmap_clear(bitmap_t* map, uint64_t index)
{
    if (index >= map->length)
    {
        return;
    }

    uint64_t qwordIdx = index / 64;
    uint64_t bitInQword = index % 64;
    map->buffer[qwordIdx] &= ~(1ULL << bitInQword);
    map->firstZeroIdx = MIN(map->firstZeroIdx, index);
}

void bitmap_clear_range(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
    {
        return;
    }

    if (low < map->firstZeroIdx)
    {
        map->firstZeroIdx = low;
    }

    uint64_t firstQwordIdx = low / 64;
    uint64_t firstBitInQword = low % 64;
    uint64_t lastQwordIdx = (high - 1) / 64;
    uint64_t lastBitInQword = (high - 1) % 64;

    if (firstQwordIdx == lastQwordIdx)
    {
        uint64_t mask = (~0ULL << firstBitInQword) & (~0ULL >> (63 - lastBitInQword));
        map->buffer[firstQwordIdx] &= ~mask;
        return;
    }

    map->buffer[firstQwordIdx] &= ~(~0ULL << firstBitInQword);

    for (uint64_t i = firstQwordIdx + 1; i < lastQwordIdx; i++)
    {
        map->buffer[i] = 0ULL;
    }

    map->buffer[lastQwordIdx] &= ~(~0ULL >> (63 - lastBitInQword));
}

uint64_t bitmap_find_first_clear(bitmap_t* map, uint64_t startIdx, uint64_t endIdx)
{
    if (map->firstZeroIdx >= map->length)
    {
        return map->length;
    }

    startIdx = MAX(startIdx, map->firstZeroIdx);
    uint64_t qwordIdx = startIdx / 64;
    uint64_t bitIdx = startIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(MIN(endIdx, map->length));

    if (bitIdx != 0)
    {
        uint64_t qword = map->buffer[qwordIdx];
        uint64_t maskedQword = qword | ((1ULL << bitIdx) - 1);
        if (maskedQword != ~0ULL)
        {
            return qwordIdx * 64 + __builtin_ctzll(~maskedQword);
        }
        qwordIdx++;
    }

    for (uint64_t i = qwordIdx; i < endQwordIdx; ++i)
    {
        if (map->buffer[i] != ~0ULL)
        {
            return i * 64 + __builtin_ctzll(~map->buffer[i]);
        }
    }

    return map->length;
}

uint64_t bitmap_find_first_set(bitmap_t* map, uint64_t startIdx, uint64_t endIdx)
{
    if (startIdx >= map->length)
    {
        return map->length;
    }

    uint64_t startQwordIdx = startIdx / 64;
    uint64_t startBitIdx = startIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(MIN(endIdx, map->length));

    while (startQwordIdx < endQwordIdx)
    {
        uint64_t qword = map->buffer[startQwordIdx];
        if (startBitIdx != 0)
        {
            qword &= ~((1ULL << startBitIdx) - 1);
        }

        if (qword != 0)
        {
            return startQwordIdx * 64 + __builtin_ctzll(qword);
        }

        startQwordIdx++;
        startBitIdx = 0;
    }

    return map->length;
}

uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t minIdx, uintptr_t maxIdx, uint64_t length,
    uint64_t alignment)
{
    if (length == 0 || minIdx >= maxIdx || maxIdx > map->length)
    {
        return map->length;
    }

    if (alignment == 0)
    {
        alignment = 1;
    }

    uint64_t idx = MAX(minIdx, map->firstZeroIdx);
    idx = ROUND_UP(idx, alignment);

    while (idx <= maxIdx - length)
    {
        uint64_t firstSet = bitmap_find_first_set(map, idx, idx + length);
        if (firstSet >= idx + length)
        {
            bitmap_set_range(map, idx, idx + length);
            return idx;
        }
        idx = ROUND_UP(firstSet + 1, alignment);
    }

    return map->length;
}