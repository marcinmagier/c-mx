
#ifndef __MX_MISC_H_
#define __MX_MISC_H_



#define UNUSED(x)           { (void)(x); }
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof((x)[0]))

#define MIN(a, b)           (((a) < (b)) ? (a) : (b))
#define MAX(a, b)           (((a) < (b)) ? (b) : (a))

#define SET_BIT(reg, bit)   ((reg) |= (bit))
#define CLEAR_BIT(reg, bit) ((reg) &= ~(bit))
#define READ_BIT(reg, bit)  ((reg) & (bit))

#define ENUM_STR(item)      [item] = #item



#endif /* __MX_MISC_H_ */
