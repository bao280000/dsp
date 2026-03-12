#ifndef NIMU_ETH_PDK2_H_
#define NIMU_ETH_PDK2_H_

#include <ti/ndk/inc/stkmain.h>
#include <ti/ndk/inc/netmain.h>

/* C6678 在 PDK 2.x 中的专属架构是 v1 (基于 PA/QMSS)，而非 v0 */
#include <ti/transport/ndk/nimu/src/v1/nimu_eth.h>

extern int EmacInit(STKEVENT_Handle hEvent);

#endif /* NIMU_ETH_PDK2_H_ */
