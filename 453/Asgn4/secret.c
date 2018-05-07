#include <minix/drivers.h>
#include <minix/driver.h>
#include <sys/ioctl.h>
#include <sys/ucred.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include <minix/const.h>
#include <errno.h>
#include <unistd.h>
#include "secret.h"

/*
 * Function prototypes for the secret driver.
 */
FORWARD _PROTOTYPE( char * secret_name,   (void) );
FORWARD _PROTOTYPE( int secret_open,      (struct driver *d, message *m) );
FORWARD _PROTOTYPE( int secret_close,     (struct driver *d, message *m) );
FORWARD _PROTOTYPE( struct device * secret_prepare, (int device) );
FORWARD _PROTOTYPE( int secret_transfer,  (int procnr, int opcode,
                                          u64_t position, iovec_t *iov,
                                          unsigned nr_req) );
FORWARD _PROTOTYPE( void secret_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( int secret_ioctl,     (struct driver *d, message *m) );

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( int sef_cb_lu_state_save, (int) );
FORWARD _PROTOTYPE( int lu_state_restore, (void) );

/* Entry points to the secret driver. */
PRIVATE struct driver secret_tab =
{
    secret_name,
    secret_open,
    secret_close,
    secret_ioctl,
    secret_prepare,
    secret_transfer,
    nop_cleanup,
    secret_geometry,
    nop_alarm,
    nop_cancel,
    nop_select,
    nop_ioctl,
    do_nop,
};

/** Represents the /dev/secret device. */
PRIVATE struct device secret_device;

/** State variable to count the number of times the device has been opened. */
PRIVATE int open_counter = 0;

/* Updates with how many bytes have already been written */
PRIVATE int bytes_written = 0;

/* Updates with how many bytes have already been read*/
PRIVATE int bytes_read = 0;

PRIVATE int write_op = 0;

/** secret message */
PRIVATE char secret[SECRET_SIZE];

/** secret owner */
PRIVATE uid_t owner;

/** state variable to keep track of whether secret is empty */
PRIVATE int is_empty = 1;

PRIVATE char * secret_name(void)
{
    return "secret";
}

PRIVATE int secret_open(d, m)
    struct driver *d;
    message *m;
{
    struct ucred proc_owner;
    int permissions = m->COUNT;

    if (getnucred(m->IO_ENDPT, &proc_owner)) {
        perror("Error: couldn't get process owner.\n");
        return -1;
    }

    if (!m) {
        perror("Error: message is null.\n");
        return -1;
    }

    if (permissions & R_BIT && permissions & W_BIT) {
        return EACCES;
    }

    if (is_empty) {
        is_empty = 0;
        owner = proc_owner.uid;
    }
    else {
        if (permissions & W_BIT) {
            return ENOSPC;
        }

        if (owner != proc_owner.uid) {
            return EACCES;
        }
    }

    if (permissions & R_BIT) {
        open_counter++;
    }
    else {
        write_op = 1;
    }

    return OK;
}

PRIVATE int secret_close(d, m)
    struct driver *d;
    message *m;
{
    if (!m) {
        perror("Error: message is null.\n");
        return -1;
    }

    if (!write_op) {
        open_counter--;
    }
    else {
        write_op = 0;
        return OK;
    }

    if (!open_counter) {
        is_empty = 1;
        bytes_written = 0;
        bytes_read = 0;
    }

    return OK;
}

PRIVATE int secret_ioctl(d, m)
    struct driver *d;
    message *m;
{
    uid_t grantee;
    int res;

    if (!m) {
        perror("Error: message is null.\n");
        exit(-1);
    }

    if (m->REQUEST != SSGRANT) {
        return ENOTTY;
    }

    res = sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)m->IO_GRANT, 0,
    (vir_bytes)&grantee, sizeof(grantee), D);

    if (res) {
        perror("Error: unable to safe copy from.\n");
        return -1;
    }

    owner = grantee;
}

PRIVATE struct device * secret_prepare(dev)
    int dev;
{
    secret_device.dv_base.lo = 0;
    secret_device.dv_base.hi = 0;
    secret_device.dv_size.lo = strlen(secret);
    secret_device.dv_size.hi = 0;
    return &secret_device;
}

PRIVATE int secret_transfer(proc_nr, opcode, position, iov, nr_req)
    int proc_nr;
    int opcode;
    u64_t position;
    iovec_t *iov;
    unsigned nr_req;
{
    int bytes, ret;

    errno = 0;
    ret = OK;
    bytes = iov->iov_size;

    switch (opcode)
    {
        case DEV_SCATTER_S:
            /* error check to make sure we have the room to
             write the amount of bytes requested*/
            if (bytes > SECRET_SIZE - bytes_written)
            {
                errno = ENOSPC;
                bytes = SECRET_SIZE - bytes_written;
            }
            ret = sys_safecopyfrom(proc_nr, iov->iov_addr, 0,
                                (vir_bytes) secret + bytes_written,
                                 bytes, D);
            iov->iov_size -= bytes;
            bytes_written += bytes;
            break;
        case DEV_GATHER_S:
            /* error check to make sure we don't try to
             read more than has been written*/
            if (bytes > bytes_written - bytes_read)
            {
                bytes = bytes_written - bytes_read;
            }
            if  (bytes > 0) {
              ret = sys_safecopyto(proc_nr, iov->iov_addr, 0,
                                (vir_bytes) secret + bytes_read,
                                 bytes, D);
              iov->iov_size -= bytes;
              bytes_read += bytes;
            }
            break;

        default:
            return EINVAL;
    }

    if (errno) {
        return errno;
    }

    if (!ret)
    {
        return bytes;
    }
    /* else ... return -1*/
    return -1;
}

PRIVATE void secret_geometry(entry)
    struct partition *entry;
{
    entry->cylinders = 0;
    entry->heads     = 0;
    entry->sectors   = 0;
}

PRIVATE int sef_cb_lu_state_save(int state) {
/* Save the state. */
    ds_publish_u32(OPEN_COUNTER, open_counter, DSF_OVERWRITE);
    ds_publish_u32(IS_EMPTY, is_empty, DSF_OVERWRITE);
    ds_publish_u32(OWNER_UID, owner, DSF_OVERWRITE);
    ds_publish_u32(BYTES_WRITTEN, bytes_written, DSF_OVERWRITE);
    ds_publish_u32(BYTES_READ, bytes_read, DSF_OVERWRITE);

    return OK;
}

PRIVATE int lu_state_restore() {
/* Restore the state. */
    u32_t value;
    struct ucred restored_owner;

    ds_retrieve_u32(OPEN_COUNTER, &value);
    ds_delete_u32(OPEN_COUNTER);
    open_counter = (int) value;

    ds_retrieve_u32(IS_EMPTY, &value);
    ds_delete_u32(IS_EMPTY);
    is_empty = (int) value;

    ds_retrieve_u32(OWNER_UID, &value);
    ds_delete_u32(OWNER_UID);
    owner = (uid_t) value;

    ds_retrieve_u32(BYTES_WRITTEN, &value);
    ds_delete_u32(BYTES_WRITTEN);
    bytes_written = (int) value;

    ds_retrieve_u32(BYTES_READ, &value);
    ds_delete_u32(BYTES_READ);
    bytes_read = (int) value;

    return OK;
}

PRIVATE void sef_local_startup()
{
    /*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

PRIVATE int sef_cb_init(int type, sef_init_info_t *info)
{
/* Initialize the secret driver. */
    int do_announce_driver = TRUE;

    open_counter = 0;
    switch(type) {
        case SEF_INIT_FRESH:
        break;

        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;
        break;

        case SEF_INIT_RESTART:
        break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        driver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

PUBLIC int main(int argc, char **argv)
{
    /*
     * Perform initialization.
     */
    sef_local_startup();

    /*
     * Run the main loop.
     */
    driver_task(&secret_tab, DRIVER_STD);
    return OK;
}
