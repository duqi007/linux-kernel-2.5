/*
 * These are too big to be inlined.
 */

#include <linux/kernel.h>
#include <asm/bitops.h>
#include <asm/byteorder.h>

unsigned long find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (64-offset);
		if (size < 64)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 64;
		result += 64;
	}
	while (size & ~63UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 64;
		size -= 64;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

static __inline__ unsigned long ___ffs(unsigned long word)
{
        unsigned long result = 0;

        while (!(word & 1UL)) {
                result++;
                word >>= 1;
        }
        return result;
}

unsigned long find_next_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < 64)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= 64;
		result += 64;
	}
	while (size & ~63UL) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += 64;
		size -= 64;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (64 - size));
	if (tmp == 0UL)        /* Are any bits set? */
		return result + size; /* Nope. */
found_middle:
	return result + ___ffs(tmp);
}

static __inline__ unsigned int ext2_ilog2(unsigned int x) 
{
	int lz;

	asm("cntlzw %0,%1" : "=r" (lz) : "r" (x));
	return 31 - lz;
}

static __inline__ unsigned int ext2_ffz(unsigned int x)
{
	u32  tempRC;
	if ((x = ~x) == 0)
		return 32;
	tempRC = ext2_ilog2(x & -x);
	return tempRC;
}

unsigned long find_next_zero_le_bit(void *addr, unsigned long size, unsigned long offset)
{
        unsigned int *p = ((unsigned int *) addr) + (offset >> 5);
        unsigned int result = offset & ~31;
        unsigned int tmp;

        if (offset >= size)
                return size;
        size -= result;
        offset &= 31;
        if (offset) {
                tmp = cpu_to_le32p(p++);
                tmp |= ~0U >> (32-offset); /* bug or feature ? */
                if (size < 32)
                        goto found_first;
                if (tmp != ~0)
                        goto found_middle;
                size -= 32;
                result += 32;
        }
        while (size >= 32) {
                if ((tmp = cpu_to_le32p(p++)) != ~0)
                        goto found_middle;
                result += 32;
                size -= 32;
        }
        if (!size)
                return result;
        tmp = cpu_to_le32p(p);
found_first:
        tmp |= ~0 << size;
        if (tmp == ~0)          /* Are any bits zero? */
                return result + size; /* Nope. */
found_middle:
        return result + ext2_ffz(tmp);
}
