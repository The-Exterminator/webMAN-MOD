#ifndef __NETISO_H__
#define __NETISO_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETISO_PORT	38008

enum NETISO_CMD
{
	/* Closes the active ro file (if any) and open/stat a new one */
	NETISO_CMD_OPEN_FILE = 0x1224,
	/* Reads the active ro file. Offsets and sizes in bytes. If file read fails, client is exited. Only read data is returned. */
	NETISO_CMD_READ_FILE_CRITICAL,
	/* Reads 2048 sectors in a 2352 sectors iso.
	 * Offsets and sizes in sectors. If file read fails, client is exited */
	NETISO_CMD_READ_CD_2048_CRITICAL,
	/* Reads the active ro file. Offsets and sizes in bytes. It returns number of bytes read to client, -1 on error, and after that, the data read (if any)
	   Only up to the BUFFER_SIZE used by server can be red at one time*/
	NETISO_CMD_READ_FILE,
	/* Closes the active wo file (if any) and opens+truncates or creates a new one */
	NETISO_CMD_CREATE_FILE,
	/* Writes to the active wo file. After command, data is sent. It returns number of bytes written to client, -1 on erro.
	   If more than BUFFER_SIZE used by server is specified in command, connection is aborted. */
	NETISO_CMD_WRITE_FILE,
	/* Closes the active directory (if any) and opens a new one */
	NETISO_CMD_OPEN_DIR,
	/* Reads a directory entry. and returns result. If no more entries or an error happens, the directory is automatically closed. . and .. are automatically ignored */
	NETISO_CMD_READ_DIR_ENTRY,
	/* Deletes a file. */
	NETISO_CMD_DELETE_FILE,
	/* Creates a directory */
	NETISO_CMD_MKDIR,
	/* Removes a directory (if empty) */
	NETISO_CMD_RMDIR,
	/* Reads a directory entry (v2). and returns result. If no more entries or an error happens, the directory is automatically closed. . and .. are automatically ignored */
	NETISO_CMD_READ_DIR_ENTRY_V2,
	/* Stats a file or directory */
	NETISO_CMD_STAT_FILE,
	/* Gets a directory size */
	NETISO_CMD_GET_DIR_SIZE,

	/* Get complete directory contents */
	NETISO_CMD_READ_DIR,

	/* Replace this with any custom command */
	NETISO_CMD_CUSTOM_0 = 0x2412,
};

typedef struct _netiso_cmd
{
	uint16_t opcode;
	uint8_t data[14];
} __attribute__((packed)) netiso_cmd;

typedef struct _netiso_stat_cmd
{
	uint16_t opcode;
	uint16_t fp_len;
	uint8_t pad[12];
} __attribute__((packed)) netiso_stat_cmd;

typedef struct _netiso_stat_result
{
	int64_t file_size; // Files: file size, directories: 0, error: -1
	uint64_t mtime;
	uint64_t ctime;
	uint64_t atime;
	int8_t is_directory; // Files: 0, directory: 1, error: shouldn't be read.
} __attribute__((packed)) netiso_stat_result;

typedef struct _netiso_read_file_cmd
{
	uint16_t opcode;
	uint16_t pad;
	uint32_t num_bytes;
	uint64_t offset;
} __attribute__((packed)) netiso_read_file_cmd;

typedef struct _netiso_read_file_result
{
	int32_t bytes_read;
} __attribute__((packed)) netiso_read_file_result;

typedef struct _netiso_open_cmd
{
	uint16_t opcode;
	uint16_t fp_len;
	uint8_t pad[12];
} __attribute__((packed)) netiso_open_cmd;

typedef struct _netiso_open_result
{
	int64_t file_size; // -1 on error
	uint64_t mtime;
} __attribute__((packed)) netiso_open_result;

typedef struct _netiso_open_dir_cmd
{
	uint16_t opcode;
	uint16_t dp_len;
	uint8_t pad[12];
} __attribute__((packed)) netiso_open_dir_cmd;

typedef struct _netiso_open_dir_result
{
	int32_t open_result; // 0 success, -1 error
} __attribute__((packed)) netiso_open_dir_result;

typedef struct _netiso_read_dir_entry_cmd
{
	uint16_t opcode;
	uint8_t pad[14];
} __attribute__((packed)) netiso_read_dir_entry_cmd;

typedef struct _netiso_read_dir_entry_result
{
	int64_t file_size; // Files: file size, directories: 0, error or no more entries: -1
	uint16_t fn_len;
	int8_t is_directory; // Files: 0, directory: 1, error: shouldn't be read.
} __attribute__((packed)) netiso_read_dir_entry_result;

typedef struct _netiso_read_dir_result
{
	int64_t dir_size;
} __attribute__((packed)) netiso_read_dir_result;

typedef struct _netiso_read_dir_result_data
{
	int64_t file_size;
	uint64_t mtime;
	int8_t is_directory;
	char name[512];
} __attribute__((packed)) netiso_read_dir_result_data;

typedef struct _netiso_read_file_critical_cmd
{
	uint16_t opcode;
	uint16_t pad;
	uint32_t num_bytes;
	uint64_t offset;
} __attribute__((packed)) netiso_read_file_critical_cmd;

typedef struct _netiso_read_cd_2048_critical_cmd
{
	uint16_t opcode;
	uint16_t pad;
	uint32_t start_sector;
	uint32_t sector_count;
	uint32_t pad2;
} __attribute__((packed)) netiso_read_cd_2048_critical_cmd;

#ifdef __BIG_ENDIAN__

static inline uint16_t BE16(uint16_t x)
{
	return x;
}

static inline uint32_t BE32(uint32_t x)
{
	return x;
}

static inline uint64_t BE64(uint64_t x)
{
	return x;
}

#else

static inline uint16_t BE16(uint16_t x)
{
	uint16_t ret = (x<<8)&0xFF00;
	ret |= ((x>>8)&0xFF);

	return ret;
}

static inline uint32_t BE32(uint32_t x)
{
	uint32_t ret = (((x) & 0xff) << 24);
	ret |= (((x) & 0xff00) << 8);
	ret |= (((x) & 0xff0000) >> 8);
	ret |= (((x) >> 24) & 0xff);

	return ret;
}

static inline uint64_t BE64(uint64_t x)
{
	uint64_t ret = (x << 56) & 0xff00000000000000ULL;
	ret |= ((x << 40) & 0x00ff000000000000ULL);
	ret |= ((x << 24) & 0x0000ff0000000000ULL);
	ret |= ((x << 8) & 0x000000ff00000000ULL);
	ret |= ((x >> 8) & 0x00000000ff000000ULL);
	ret |= ((x >> 24) & 0x0000000000ff0000ULL);
	ret |= ((x >> 40) & 0x000000000000ff00ULL);
	ret |= ((x >> 56) & 0x00000000000000ffULL);

	return ret;
}

#endif

#ifdef __cplusplus
}
#endif

#endif

