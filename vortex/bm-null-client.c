#include <vortex.h>

#define NULL_PROFILE "http://example.com/BEEP/NULL"

int main (int argc, char ** argv)
{
    char* port = "1976";
    char* profile = NULL_PROFILE;
    VortexConnection * connection;
    VortexChannel * channel;
    VortexFrame * frame;
    WaitReplyData * wait_reply;
    int msg_no;

    /* init vortex library */
    vortex_init ();

    switch(argc)
    {
        case 3:
            profile = argv[2];
        case 2:
            port = argv[1];
    }

    /* creates a new connection against localhost:44000 */
    printf ("connecting to localhost:%s, profile=%s\n", port, profile);
    connection = vortex_connection_new ("localhost", port, NULL, NULL);
    if (!vortex_connection_is_ok (connection, false)) {
        printf ("Unable to connect remote server, error was: %s\n",
                vortex_connection_get_message (connection));
        goto end;
    }
    printf ("ok\n");

    /* create a new channel (by chosing 0 as channel number the
     * Vortex Library will automatically assign the new channel
     * number free. */
    channel = vortex_channel_new (connection, 0,
            profile,
            /* no close handling */
            NULL, NULL,
            /* no frame receive async
             * handling */
            NULL, NULL,
            /* no async channel creation */
            NULL, NULL);
    if (channel == NULL) {
        printf ("Unable to create the channel..\n");
        goto end;
    }

    /* create a wait reply */
    wait_reply = vortex_channel_create_wait_reply ();

    /* now send the message using msg_and_wait/v */
    if (!vortex_channel_send_msg_and_wait (channel, "my message", strlen ("my message"), &msg_no, wait_reply)) {
        printf ("Unable to send my message\n");
        vortex_channel_free_wait_reply (wait_reply);
        vortex_channel_close (channel, NULL);
        goto end;

    }

    /* get blocked until the reply arrives, the wait_reply object
     * must not be freed after this function because it already
     * free it. */
    frame = vortex_channel_wait_reply (channel, msg_no, wait_reply);
    if (frame == NULL) {
        printf ("there was an error while receiving the reply or a timeout have occur\n");
        vortex_channel_close (channel, NULL);
        goto end;
    }
    printf ("my reply have arrived: (size: %d):\n%s\n", 
            vortex_frame_get_payload_size (frame), 
            vortex_frame_get_payload (frame));

end:				 
    vortex_connection_close (connection);
    vortex_exit ();
    return 0 ;	 
}

