//
// Created by Max Wang on 2026/1/18.
//

#ifndef TIME_MMIO_REGISTER_H
#define TIME_MMIO_REGISTER_H
//0x001800          | 0x001817    | 28 B
#include "../../vm.h"
void register_time_mmio(VM *vm);
#endif //VM_MMIO_REGISTER_H