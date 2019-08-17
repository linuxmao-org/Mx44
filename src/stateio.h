#ifndef _STATEIO_H_
#define _STATEIO_H_

#include "mx44.h"

void load_state(Mx44state* mx44,
                const char* load_from,
                const char* save_to);

void save_state(Mx44state* mx44);

#endif /* _STATEIO_H_ */
