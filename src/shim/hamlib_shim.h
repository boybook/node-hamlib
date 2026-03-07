/**
 * hamlib_shim.h - Pure C interface layer for Hamlib
 *
 * This shim provides a compiler-safe C ABI boundary between the Node.js
 * native addon (compiled with MSVC on Windows) and the Hamlib library
 * (compiled with MinGW on Windows).
 *
 * On Linux/macOS: compiled as static library (.a), linked into addon
 * On Windows: compiled as DLL (MinGW), loaded by addon (MSVC)
 *
 * All Hamlib struct access is encapsulated here to avoid cross-compiler
 * struct layout differences.
 */

#ifndef HAMLIB_SHIM_H
#define HAMLIB_SHIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DLL export/import macros */
#ifdef _WIN32
  #ifdef HAMLIB_SHIM_BUILD
    #define SHIM_API __declspec(dllexport)
  #else
    #define SHIM_API __declspec(dllimport)
  #endif
#else
  #define SHIM_API
#endif

/* Opaque handle type - hides RIG* from the addon */
typedef void* hamlib_shim_handle_t;

/* ===== Constants (mirror Hamlib values) ===== */

/* Return codes */
#define SHIM_RIG_OK          0
#define SHIM_RIG_EINVAL     -1
#define SHIM_RIG_EIO        -2
#define SHIM_RIG_ENIMPL     -3
#define SHIM_RIG_ETIMEOUT   -4
#define SHIM_RIG_ENAVAIL    -11
#define SHIM_RIG_EPROTO     -8

/* VFO constants */
#define SHIM_RIG_VFO_NONE    0
#define SHIM_RIG_VFO_CURR    ((int)0x40000000)  /* RIG_VFO_CURR */
#define SHIM_RIG_VFO_MEM     ((int)0x10000000)  /* RIG_VFO_MEM */
#define SHIM_RIG_VFO_A       ((int)(1<<0))      /* RIG_VFO_A */
#define SHIM_RIG_VFO_B       ((int)(1<<1))      /* RIG_VFO_B */

/* Mode constants */
#define SHIM_RIG_MODE_NONE   0

/* Split constants */
#define SHIM_RIG_SPLIT_OFF   0
#define SHIM_RIG_SPLIT_ON    1

/* PTT constants */
#define SHIM_RIG_PTT_OFF     0
#define SHIM_RIG_PTT_ON      1

/* DCD constants */
#define SHIM_RIG_DCD_OFF     0
#define SHIM_RIG_DCD_ON      1

/* Passband constants */
#define SHIM_RIG_PASSBAND_NORMAL  0

/* Port types */
#define SHIM_RIG_PORT_SERIAL   0
#define SHIM_RIG_PORT_NETWORK  4

/* Scan types */
#define SHIM_RIG_SCAN_STOP   0
#define SHIM_RIG_SCAN_MEM    1
#define SHIM_RIG_SCAN_VFO    (1<<1)
#define SHIM_RIG_SCAN_PROG   (1<<2)
#define SHIM_RIG_SCAN_DELTA  (1<<3)
#define SHIM_RIG_SCAN_PRIO   (1<<4)

/* Transceive mode */
#define SHIM_RIG_TRN_POLL    1

/* Power status */
#define SHIM_RIG_POWER_UNKNOWN -1

/* PTT types (for port config) */
#define SHIM_RIG_PTT_NONE         0
#define SHIM_RIG_PTT_RIG          1
#define SHIM_RIG_PTT_SERIAL_DTR   2
#define SHIM_RIG_PTT_SERIAL_RTS   3
#define SHIM_RIG_PTT_PARALLEL     4
#define SHIM_RIG_PTT_CM108        8
#define SHIM_RIG_PTT_GPIO         16
#define SHIM_RIG_PTT_GPION        32

/* DCD types (for port config) */
#define SHIM_RIG_DCD_NONE         0
#define SHIM_RIG_DCD_RIG          1
#define SHIM_RIG_DCD_SERIAL_DSR   2
#define SHIM_RIG_DCD_SERIAL_CTS   3
#define SHIM_RIG_DCD_SERIAL_CAR   4
#define SHIM_RIG_DCD_PARALLEL     5
#define SHIM_RIG_DCD_CM108        8
#define SHIM_RIG_DCD_GPIO         16
#define SHIM_RIG_DCD_GPION        32

/* Serial parity */
#define SHIM_RIG_PARITY_NONE   0
#define SHIM_RIG_PARITY_ODD    1
#define SHIM_RIG_PARITY_EVEN   2

/* Serial handshake */
#define SHIM_RIG_HANDSHAKE_NONE      0
#define SHIM_RIG_HANDSHAKE_XONXOFF   1
#define SHIM_RIG_HANDSHAKE_HARDWARE  2

/* Serial control state */
#define SHIM_RIG_SIGNAL_UNSET  0
#define SHIM_RIG_SIGNAL_ON     1
#define SHIM_RIG_SIGNAL_OFF    2

/* Repeater shift */
#define SHIM_RIG_RPT_SHIFT_NONE  0

/* Reset types */
#define SHIM_RIG_RESET_NONE     0
#define SHIM_RIG_RESET_SOFT     1
#define SHIM_RIG_RESET_VFO      2
#define SHIM_RIG_RESET_MCALL    4
#define SHIM_RIG_RESET_MASTER   8

/* Antenna */
#define SHIM_RIG_ANT_CURR  0

/* Rig type mask and types */
#define SHIM_RIG_TYPE_MASK  0x7F000000

/* Max path length */
#define SHIM_HAMLIB_FILPATHLEN  512

/* Max modes for iteration */
#define SHIM_HAMLIB_MAX_MODES 64

/* ===== Level constants (bit positions for setting_t / uint64_t) ===== */
#define SHIM_RIG_LEVEL_PREAMP       (1ULL << 0)
#define SHIM_RIG_LEVEL_ATT          (1ULL << 1)
#define SHIM_RIG_LEVEL_VOXDELAY     (1ULL << 2)
#define SHIM_RIG_LEVEL_AF           (1ULL << 3)
#define SHIM_RIG_LEVEL_RF           (1ULL << 4)
#define SHIM_RIG_LEVEL_SQL          (1ULL << 5)
#define SHIM_RIG_LEVEL_IF           (1ULL << 6)
#define SHIM_RIG_LEVEL_APF          (1ULL << 7)
#define SHIM_RIG_LEVEL_NR           (1ULL << 8)
#define SHIM_RIG_LEVEL_PBT_IN       (1ULL << 9)
#define SHIM_RIG_LEVEL_PBT_OUT      (1ULL << 10)
#define SHIM_RIG_LEVEL_CWPITCH      (1ULL << 11)
#define SHIM_RIG_LEVEL_RFPOWER      (1ULL << 12)
#define SHIM_RIG_LEVEL_MICGAIN      (1ULL << 13)
#define SHIM_RIG_LEVEL_KEYSPD       (1ULL << 14)
#define SHIM_RIG_LEVEL_NOTCHF       (1ULL << 15)
#define SHIM_RIG_LEVEL_COMP         (1ULL << 16)
#define SHIM_RIG_LEVEL_AGC          (1ULL << 17)
#define SHIM_RIG_LEVEL_BKINDL       (1ULL << 18)
#define SHIM_RIG_LEVEL_BALANCE      (1ULL << 19)
#define SHIM_RIG_LEVEL_METER        (1ULL << 20)
#define SHIM_RIG_LEVEL_VOXGAIN      (1ULL << 21)
#define SHIM_RIG_LEVEL_ANTIVOX      (1ULL << 22)
#define SHIM_RIG_LEVEL_STRENGTH     (1ULL << 26)
#define SHIM_RIG_LEVEL_RAWSTR       (1ULL << 28)
#define SHIM_RIG_LEVEL_SWR          (1ULL << 29)
#define SHIM_RIG_LEVEL_ALC          (1ULL << 30)
#define SHIM_RIG_LEVEL_RFPOWER_METER (1ULL << 33)
#define SHIM_RIG_LEVEL_COMP_METER   (1ULL << 34)
#define SHIM_RIG_LEVEL_VD_METER     (1ULL << 35)
#define SHIM_RIG_LEVEL_ID_METER     (1ULL << 36)
#define SHIM_RIG_LEVEL_TEMP_METER   (1ULL << 42)

/* ===== Function constants (bit positions for setting_t / uint64_t) ===== */
#define SHIM_RIG_FUNC_FAGC    (1ULL << 0)
#define SHIM_RIG_FUNC_NB      (1ULL << 1)
#define SHIM_RIG_FUNC_COMP    (1ULL << 2)
#define SHIM_RIG_FUNC_VOX     (1ULL << 3)
#define SHIM_RIG_FUNC_TONE    (1ULL << 4)
#define SHIM_RIG_FUNC_TSQL    (1ULL << 5)
#define SHIM_RIG_FUNC_SBKIN   (1ULL << 6)
#define SHIM_RIG_FUNC_FBKIN   (1ULL << 7)
#define SHIM_RIG_FUNC_ANF     (1ULL << 8)
#define SHIM_RIG_FUNC_NR      (1ULL << 9)
#define SHIM_RIG_FUNC_AIP     (1ULL << 10)
#define SHIM_RIG_FUNC_APF     (1ULL << 11)
#define SHIM_RIG_FUNC_MON     (1ULL << 12)
#define SHIM_RIG_FUNC_MN      (1ULL << 13)
#define SHIM_RIG_FUNC_RF      (1ULL << 14)
#define SHIM_RIG_FUNC_ARO     (1ULL << 15)
#define SHIM_RIG_FUNC_LOCK    (1ULL << 16)
#define SHIM_RIG_FUNC_MUTE    (1ULL << 17)
#define SHIM_RIG_FUNC_VSC     (1ULL << 18)
#define SHIM_RIG_FUNC_REV     (1ULL << 19)
#define SHIM_RIG_FUNC_SQL     (1ULL << 20)
#define SHIM_RIG_FUNC_ABM     (1ULL << 21)
#define SHIM_RIG_FUNC_BC      (1ULL << 22)
#define SHIM_RIG_FUNC_MBC     (1ULL << 23)
#define SHIM_RIG_FUNC_RIT     (1ULL << 24)
#define SHIM_RIG_FUNC_AFC     (1ULL << 25)
#define SHIM_RIG_FUNC_SATMODE (1ULL << 26)
#define SHIM_RIG_FUNC_SCOPE   (1ULL << 27)
#define SHIM_RIG_FUNC_RESUME  (1ULL << 28)
#define SHIM_RIG_FUNC_TBURST  (1ULL << 29)
#define SHIM_RIG_FUNC_TUNER   (1ULL << 30)
#define SHIM_RIG_FUNC_XIT     (1ULL << 31)

/* ===== VFO operation constants ===== */
#define SHIM_RIG_OP_CPY       (1<<0)
#define SHIM_RIG_OP_XCHG      (1<<1)
#define SHIM_RIG_OP_FROM_VFO   (1<<2)
#define SHIM_RIG_OP_TO_VFO     (1<<3)
#define SHIM_RIG_OP_MCL        (1<<4)
#define SHIM_RIG_OP_UP         (1<<5)
#define SHIM_RIG_OP_DOWN       (1<<6)
#define SHIM_RIG_OP_BAND_UP    (1<<7)
#define SHIM_RIG_OP_BAND_DOWN  (1<<8)
#define SHIM_RIG_OP_LEFT       (1<<9)
#define SHIM_RIG_OP_RIGHT      (1<<10)
#define SHIM_RIG_OP_TUNE       (1<<11)
#define SHIM_RIG_OP_TOGGLE     (1<<12)

/* ===== Repeater shift constants ===== */
#define SHIM_RIG_RPT_SHIFT_MINUS 1
#define SHIM_RIG_RPT_SHIFT_PLUS  2

/* ===== Simplified structs for cross-compiler safety ===== */

/* Simplified channel data for set/get memory channel */
typedef struct {
    int channel_num;
    double freq;
    double tx_freq;
    int mode;          /* rmode_t as int */
    int width;         /* pbwidth_t as int */
    int split;         /* split_t as int */
    int ctcss_tone;
    int vfo;
    char channel_desc[64];
} shim_channel_t;

/* Rig info for rig list enumeration */
typedef struct {
    unsigned int rig_model;
    const char* model_name;
    const char* mfg_name;
    const char* version;
    int status;
    int rig_type;
} shim_rig_info_t;

/* ===== Callback types ===== */

/* Frequency change callback: (handle, vfo, freq, arg) -> int */
typedef int (*shim_freq_cb_t)(void* handle, int vfo, double freq, void* arg);

/* PTT change callback: (handle, vfo, ptt, arg) -> int */
typedef int (*shim_ptt_cb_t)(void* handle, int vfo, int ptt, void* arg);

/* Rig list callback: (info, data) -> int */
typedef int (*shim_rig_list_cb_t)(const shim_rig_info_t* info, void* data);

/* ===== Lifecycle functions ===== */

SHIM_API hamlib_shim_handle_t shim_rig_init(unsigned int model);
SHIM_API int  shim_rig_open(hamlib_shim_handle_t h);
SHIM_API int  shim_rig_close(hamlib_shim_handle_t h);
SHIM_API int  shim_rig_cleanup(hamlib_shim_handle_t h);
SHIM_API const char* shim_rigerror(int errcode);

/* ===== Debug / Utility ===== */

SHIM_API void shim_rig_set_debug(int level);
SHIM_API int  shim_rig_get_debug(void);
SHIM_API const char* shim_rig_get_version(void);
SHIM_API int  shim_rig_load_all_backends(void);
SHIM_API int  shim_rig_list_foreach(shim_rig_list_cb_t cb, void* data);
SHIM_API const char* shim_rig_strstatus(int status);

/* ===== Port configuration (before open) ===== */

SHIM_API void shim_rig_set_port_path(hamlib_shim_handle_t h, const char* path);
SHIM_API void shim_rig_set_port_type(hamlib_shim_handle_t h, int type);

/* Serial port configuration */
SHIM_API void shim_rig_set_serial_rate(hamlib_shim_handle_t h, int rate);
SHIM_API int  shim_rig_get_serial_rate(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_data_bits(hamlib_shim_handle_t h, int bits);
SHIM_API int  shim_rig_get_serial_data_bits(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_stop_bits(hamlib_shim_handle_t h, int bits);
SHIM_API int  shim_rig_get_serial_stop_bits(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_parity(hamlib_shim_handle_t h, int parity);
SHIM_API int  shim_rig_get_serial_parity(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_handshake(hamlib_shim_handle_t h, int handshake);
SHIM_API int  shim_rig_get_serial_handshake(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_rts_state(hamlib_shim_handle_t h, int state);
SHIM_API int  shim_rig_get_serial_rts_state(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_serial_dtr_state(hamlib_shim_handle_t h, int state);
SHIM_API int  shim_rig_get_serial_dtr_state(hamlib_shim_handle_t h);

/* Port timing configuration */
SHIM_API void shim_rig_set_port_timeout(hamlib_shim_handle_t h, int ms);
SHIM_API int  shim_rig_get_port_timeout(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_port_retry(hamlib_shim_handle_t h, int count);
SHIM_API int  shim_rig_get_port_retry(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_port_write_delay(hamlib_shim_handle_t h, int ms);
SHIM_API int  shim_rig_get_port_write_delay(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_port_post_write_delay(hamlib_shim_handle_t h, int ms);
SHIM_API int  shim_rig_get_port_post_write_delay(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_port_flushx(hamlib_shim_handle_t h, int enable);
SHIM_API int  shim_rig_get_port_flushx(hamlib_shim_handle_t h);

/* PTT/DCD port type configuration */
SHIM_API void shim_rig_set_ptt_type(hamlib_shim_handle_t h, int type);
SHIM_API int  shim_rig_get_ptt_type(hamlib_shim_handle_t h);

SHIM_API void shim_rig_set_dcd_type(hamlib_shim_handle_t h, int type);
SHIM_API int  shim_rig_get_dcd_type(hamlib_shim_handle_t h);

/* ===== Frequency control ===== */

SHIM_API int shim_rig_set_freq(hamlib_shim_handle_t h, int vfo, double freq);
SHIM_API int shim_rig_get_freq(hamlib_shim_handle_t h, int vfo, double* freq);

/* ===== VFO control ===== */

SHIM_API int shim_rig_set_vfo(hamlib_shim_handle_t h, int vfo);
SHIM_API int shim_rig_get_vfo(hamlib_shim_handle_t h, int* vfo);

/* ===== Mode control ===== */

SHIM_API int shim_rig_set_mode(hamlib_shim_handle_t h, int vfo, int mode, int width);
SHIM_API int shim_rig_get_mode(hamlib_shim_handle_t h, int vfo, int* mode, int* width);
SHIM_API int shim_rig_parse_mode(const char* mode_str);
SHIM_API const char* shim_rig_strrmode(int mode);
SHIM_API int shim_rig_passband_narrow(hamlib_shim_handle_t h, int mode);
SHIM_API int shim_rig_passband_wide(hamlib_shim_handle_t h, int mode);

/* ===== PTT control ===== */

SHIM_API int shim_rig_set_ptt(hamlib_shim_handle_t h, int vfo, int ptt);
SHIM_API int shim_rig_get_ptt(hamlib_shim_handle_t h, int vfo, int* ptt);
SHIM_API int shim_rig_get_dcd(hamlib_shim_handle_t h, int vfo, int* dcd);

/* ===== Signal strength ===== */

SHIM_API int shim_rig_get_strength(hamlib_shim_handle_t h, int vfo, int* strength);

/* ===== Level control ===== */

SHIM_API int shim_rig_set_level_f(hamlib_shim_handle_t h, int vfo, uint64_t level, float value);
SHIM_API int shim_rig_set_level_i(hamlib_shim_handle_t h, int vfo, uint64_t level, int value);
SHIM_API int shim_rig_get_level_f(hamlib_shim_handle_t h, int vfo, uint64_t level, float* value);
SHIM_API int shim_rig_get_level_i(hamlib_shim_handle_t h, int vfo, uint64_t level, int* value);

/* ===== Function control ===== */

SHIM_API int shim_rig_set_func(hamlib_shim_handle_t h, int vfo, uint64_t func, int enable);
SHIM_API int shim_rig_get_func(hamlib_shim_handle_t h, int vfo, uint64_t func, int* state);

/* ===== Parameter control ===== */

SHIM_API int shim_rig_set_parm_f(hamlib_shim_handle_t h, uint64_t parm, float value);
SHIM_API int shim_rig_set_parm_i(hamlib_shim_handle_t h, uint64_t parm, int value);
SHIM_API int shim_rig_get_parm_f(hamlib_shim_handle_t h, uint64_t parm, float* value);
SHIM_API int shim_rig_get_parm_i(hamlib_shim_handle_t h, uint64_t parm, int* value);
SHIM_API uint64_t shim_rig_parse_parm(const char* parm_str);

/* ===== Split operations ===== */

SHIM_API int shim_rig_set_split_freq(hamlib_shim_handle_t h, int vfo, double freq);
SHIM_API int shim_rig_get_split_freq(hamlib_shim_handle_t h, int vfo, double* freq);

SHIM_API int shim_rig_set_split_vfo(hamlib_shim_handle_t h, int rx_vfo, int split, int tx_vfo);
SHIM_API int shim_rig_get_split_vfo(hamlib_shim_handle_t h, int vfo, int* split, int* tx_vfo);

SHIM_API int shim_rig_set_split_mode(hamlib_shim_handle_t h, int vfo, int mode, int width);
SHIM_API int shim_rig_get_split_mode(hamlib_shim_handle_t h, int vfo, int* mode, int* width);

SHIM_API int shim_rig_set_split_freq_mode(hamlib_shim_handle_t h, int vfo, double freq, int mode, int width);
SHIM_API int shim_rig_get_split_freq_mode(hamlib_shim_handle_t h, int vfo, double* freq, int* mode, int* width);

/* ===== RIT/XIT control ===== */

SHIM_API int shim_rig_set_rit(hamlib_shim_handle_t h, int vfo, int offset);
SHIM_API int shim_rig_get_rit(hamlib_shim_handle_t h, int vfo, int* offset);
SHIM_API int shim_rig_set_xit(hamlib_shim_handle_t h, int vfo, int offset);
SHIM_API int shim_rig_get_xit(hamlib_shim_handle_t h, int vfo, int* offset);

/* ===== Memory channel operations ===== */

SHIM_API int shim_rig_set_channel(hamlib_shim_handle_t h, int vfo, const shim_channel_t* chan);
SHIM_API int shim_rig_get_channel(hamlib_shim_handle_t h, int vfo, shim_channel_t* chan, int read_only);
SHIM_API int shim_rig_set_mem(hamlib_shim_handle_t h, int vfo, int ch);
SHIM_API int shim_rig_get_mem(hamlib_shim_handle_t h, int vfo, int* ch);
SHIM_API int shim_rig_set_bank(hamlib_shim_handle_t h, int vfo, int bank);
SHIM_API int shim_rig_mem_count(hamlib_shim_handle_t h);

/* ===== Scanning ===== */

SHIM_API int shim_rig_scan(hamlib_shim_handle_t h, int vfo, int scan_type, int channel);

/* ===== VFO operations ===== */

SHIM_API int shim_rig_vfo_op(hamlib_shim_handle_t h, int vfo, int op);

/* ===== Antenna control ===== */

SHIM_API int shim_rig_set_ant(hamlib_shim_handle_t h, int vfo, int ant, float option);
SHIM_API int shim_rig_get_ant(hamlib_shim_handle_t h, int vfo, int ant, float* option,
                               int* ant_curr, int* ant_tx, int* ant_rx);

/* ===== Tuning step ===== */

SHIM_API int shim_rig_set_ts(hamlib_shim_handle_t h, int vfo, int ts);
SHIM_API int shim_rig_get_ts(hamlib_shim_handle_t h, int vfo, int* ts);

/* ===== Repeater control ===== */

SHIM_API int shim_rig_set_rptr_shift(hamlib_shim_handle_t h, int vfo, int shift);
SHIM_API int shim_rig_get_rptr_shift(hamlib_shim_handle_t h, int vfo, int* shift);
SHIM_API const char* shim_rig_strptrshift(int shift);
SHIM_API int shim_rig_set_rptr_offs(hamlib_shim_handle_t h, int vfo, int offset);
SHIM_API int shim_rig_get_rptr_offs(hamlib_shim_handle_t h, int vfo, int* offset);

/* ===== CTCSS/DCS tone control ===== */

SHIM_API int shim_rig_set_ctcss_tone(hamlib_shim_handle_t h, int vfo, unsigned int tone);
SHIM_API int shim_rig_get_ctcss_tone(hamlib_shim_handle_t h, int vfo, unsigned int* tone);
SHIM_API int shim_rig_set_dcs_code(hamlib_shim_handle_t h, int vfo, unsigned int code);
SHIM_API int shim_rig_get_dcs_code(hamlib_shim_handle_t h, int vfo, unsigned int* code);
SHIM_API int shim_rig_set_ctcss_sql(hamlib_shim_handle_t h, int vfo, unsigned int tone);
SHIM_API int shim_rig_get_ctcss_sql(hamlib_shim_handle_t h, int vfo, unsigned int* tone);
SHIM_API int shim_rig_set_dcs_sql(hamlib_shim_handle_t h, int vfo, unsigned int code);
SHIM_API int shim_rig_get_dcs_sql(hamlib_shim_handle_t h, int vfo, unsigned int* code);

/* ===== DTMF ===== */

SHIM_API int shim_rig_send_dtmf(hamlib_shim_handle_t h, int vfo, const char* digits);
SHIM_API int shim_rig_recv_dtmf(hamlib_shim_handle_t h, int vfo, char* digits, int* length);

/* ===== Morse code ===== */

SHIM_API int shim_rig_send_morse(hamlib_shim_handle_t h, int vfo, const char* msg);
SHIM_API int shim_rig_stop_morse(hamlib_shim_handle_t h, int vfo);
SHIM_API int shim_rig_wait_morse(hamlib_shim_handle_t h, int vfo);

/* ===== Voice memory ===== */

SHIM_API int shim_rig_send_voice_mem(hamlib_shim_handle_t h, int vfo, int ch);
SHIM_API int shim_rig_stop_voice_mem(hamlib_shim_handle_t h, int vfo);

/* ===== Power control ===== */

SHIM_API int shim_rig_set_powerstat(hamlib_shim_handle_t h, int status);
SHIM_API int shim_rig_get_powerstat(hamlib_shim_handle_t h, int* status);

/* ===== Power conversion ===== */

SHIM_API int shim_rig_power2mW(hamlib_shim_handle_t h, unsigned int* mwpower, float power, double freq, int mode);
SHIM_API int shim_rig_mW2power(hamlib_shim_handle_t h, float* power, unsigned int mwpower, double freq, int mode);

/* ===== Reset ===== */

SHIM_API int shim_rig_reset(hamlib_shim_handle_t h, int reset_type);

/* ===== Callbacks ===== */

SHIM_API int shim_rig_set_freq_callback(hamlib_shim_handle_t h, shim_freq_cb_t cb, void* arg);
SHIM_API int shim_rig_set_ptt_callback(hamlib_shim_handle_t h, shim_ptt_cb_t cb, void* arg);
SHIM_API int shim_rig_set_trn(hamlib_shim_handle_t h, int trn);

/* ===== Capability queries (replaces direct caps-> access) ===== */

SHIM_API uint64_t shim_rig_get_mode_list(hamlib_shim_handle_t h);
SHIM_API uint64_t shim_rig_get_caps_has_get_level(hamlib_shim_handle_t h);
SHIM_API uint64_t shim_rig_get_caps_has_set_level(hamlib_shim_handle_t h);
SHIM_API uint64_t shim_rig_get_caps_has_get_func(hamlib_shim_handle_t h);
SHIM_API uint64_t shim_rig_get_caps_has_set_func(hamlib_shim_handle_t h);

/* Format mode bitmask to string list */
SHIM_API int shim_rig_sprintf_mode(uint64_t modes, char* buf, int buflen);

/* Rig type to string conversion */
SHIM_API const char* shim_rig_type_str(int rig_type);

#ifdef __cplusplus
}
#endif

#endif /* HAMLIB_SHIM_H */
