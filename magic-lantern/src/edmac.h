#ifndef _edmac_c
#define _edmac_c

#define EDMAC_WRITE_0  0
#define EDMAC_WRITE_1  1
#define EDMAC_WRITE_2  2
#define EDMAC_WRITE_3  3
#define EDMAC_WRITE_4  4
#define EDMAC_WRITE_5  5
#define EDMAC_WRITE_6  6
#define EDMAC_READ_0   8
#define EDMAC_READ_1   9
#define EDMAC_READ_2   10
#define EDMAC_READ_3   11
#define EDMAC_READ_4   12
#define EDMAC_READ_5   13
#define EDMAC_WRITE_7  16
#define EDMAC_WRITE_8  17
#define EDMAC_WRITE_9  18
#define EDMAC_WRITE_10 19
#define EDMAC_WRITE_11 20
#define EDMAC_WRITE_12 21
#define EDMAC_WRITE_13 22
#define EDMAC_READ_6   24
#define EDMAC_READ_7   25
#define EDMAC_READ_8   26
#define EDMAC_READ_9   27
#define EDMAC_READ_10  28
#define EDMAC_READ_11  29
#define EDMAC_WRITE_14 33
#define EDMAC_WRITE_15 34
#define EDMAC_READ_12  40
#define EDMAC_READ_13  41
#define EDMAC_READ_14  42
#define EDMAC_READ_15  43

#define EDMAC_DIR_READ    0
#define EDMAC_DIR_WRITE   1
#define EDMAC_DIR_UNUSED  2

struct edmac_info
{
    unsigned int off1a;
    unsigned int off1b;
    unsigned int off2a;
    unsigned int off2b;
    unsigned int off3;
    unsigned int xa;
    unsigned int xb;
    unsigned int ya;
    unsigned int yb;
    unsigned int xn;
    unsigned int yn;
};


void EDMAC_Register_Complete_CBR(unsigned int channel, void (*cbr)(), unsigned int ctx);
void SetEDmac(unsigned int channel, void *address, struct edmac_info *ptr, int flags);
void StartEDmac(unsigned int channel, int flags);
void ConnectWriteEDmac(unsigned int channel, unsigned int where);
void ConnectReadEDmac(unsigned int channel, unsigned int where);
unsigned int GetEdmacAddress(unsigned int channel);

uint32_t edmac_channel_to_index(uint32_t channel);
uint32_t edmac_index_to_channel(uint32_t index, uint32_t direction);

uint32_t edmac_get_flags(uint32_t channel);
uint32_t edmac_get_state(uint32_t channel);
uint32_t edmac_get_base(uint32_t channel);
uint32_t edmac_get_address(uint32_t channel);
uint32_t edmac_get_length(uint32_t channel);
uint32_t edmac_get_connection(uint32_t channel, uint32_t direction);
uint32_t edmac_get_dir(uint32_t channel);

struct LockEntry *CreateResLockEntry(uint32_t *resIds, uint32_t resIdCount);
unsigned int LockEngineResources(struct LockEntry *lockEntry);
unsigned int UnLockEngineResources(struct LockEntry *lockEntry);

void RegisterEDmacCompleteCBR(int channel, void (*cbr)(void*), void* cbr_ctx);
void RegisterEDmacAbortCBR(int channel, void (*cbr)(void*), void* cbr_ctx);
void RegisterEDmacPopCBR(int channel, void (*cbr)(void*), void* cbr_ctx);

void UnregisterEDmacCompleteCBR(int channel);
void UnregisterEDmacAbortCBR(int channel);
void UnregisterEDmacPopCBR(int channel);

#endif
