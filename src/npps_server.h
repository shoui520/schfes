#pragma once
/* In-process NPPS4 server (src/server). It runs on its own thread pinned to
 * CPU2 and listens on 127.0.0.1:SIF_SERVER_PORT; the game reaches it through
 * the server_info.json written by sif_server_start. */
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SIF_SERVER_PORT 51376
/* Game threads may use these cores; CPU2 is reserved for the server. */
#define SIF_GAME_CPU_MASK 0x00030000
#define SIF_SERVER_CPU_MASK 0x00040000
/* Starts the server thread. Returns 0 when the listening socket is open;
 * database preparation continues on the server thread. */
int sif_server_start(void);
/* config/server_info.json for the local server, honky v3 encrypted. The
 * caller frees *out. Returns 0 on success. */
int sif_server_info_file(unsigned char **out, size_t *size);
#ifdef __cplusplus
}
#endif
