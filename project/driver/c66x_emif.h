#ifndef _C66X_EMIF_H_
#define _C66X_EMIF_H_

#include <stdint.h>
#include <ti/csl/cslr_emif16.h>

/* CS3 base address for FPGA EMIF16 interface */
#define CS3_MEMORY_DATA_ADDR 0x74000000

void emif16_init(void);

#endif /* _C66X_EMIF_H_ */
