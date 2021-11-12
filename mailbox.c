#define mailbox_c

#include "mailbox.h"

#define NO_MAILBOXES 30

static void *shared_memory = NULL;
static mailbox *freelist = NULL;  /* list of free mailboxes.  */


/*
 *  initialise the data structures of mailbox.  Assign prev to the
 *  mailbox prev field.
 */

static mailbox *mailbox_config (mailbox *mbox, mailbox *prev)
{
  mbox->in = 0; // initialising the in and out variables added to mailbox.h
  mbox->out = 0; // these replaced the initialisation of the data variables
  mbox->prev = prev;
  mbox->item_available = multiprocessor_initSem (0); // initialising the semaphores used in monitoring the processors
  mbox->space_available = multiprocessor_initSem (1); // and the mutex access
  mbox->mutex = multiprocessor_initSem (1);
  return mbox;
}


/*
 *  init_memory - initialise the shared memory region once.
 *                It also initialises all mailboxes.
 */

static void init_memory (void)
{
  if (shared_memory == NULL)
    {
      mailbox *mbox;
      mailbox *prev = NULL;
      int i;
      _M2_multiprocessor_init ();
      shared_memory = multiprocessor_initSharedMemory
	(NO_MAILBOXES * sizeof (mailbox));
      mbox = shared_memory;
      for (i = 0; i < NO_MAILBOXES; i++)
	prev = mailbox_config (&mbox[i], prev);
      freelist = prev;
    }
}


/*
 *  init - create a single mailbox which can contain a single triple.
 */

mailbox *mailbox_init (void)
{
  mailbox *mbox;

  init_memory ();
  if (freelist == NULL)
    {
      printf ("exhausted mailboxes\n");
      exit (1);
    }
  mbox = freelist;
  freelist = freelist->prev;
  return mbox;
}


/*
 *  kill - return the mailbox to the freelist.  No process must use this
 *         mailbox.
 */

mailbox *mailbox_kill (mailbox *mbox)
{
  mbox->prev = freelist;
  freelist = mbox;
  return NULL;
}


/*
 *  send - send (result, move_no, positions_explored) to the mailbox mbox.
 */

void mailbox_send (mailbox *mbox, int result, int move_no, int positions_explored)
{
  multiprocessor_wait (mbox->space_available); //wait for a signal for when there is space in the mailbox to send data
  multiprocessor_wait (mbox->mutex);	//wait for a signal for the mailbox to be opened from the parent
  mbox->data[mbox->in].result = result; // These three lines are the child sending their
  mbox->data[mbox->in].move_no = move_no;// data into the mailbox buffer.
  mbox->data[mbox->in].positions_explored = positions_explored;
  mbox->in = (mbox->in + 1) % MAX_MAILBOX_DATA; // Increments the pointer for the next buffer spot by one. If it's 100, the modulus resets it back to 0
  multiprocessor_signal (mbox->mutex);			// Sends a signal to the parent that they can open the mailbox to recieve data
  multiprocessor_signal (mbox->item_available); // Send signals that a new item in the mailbox is ready to read
}


/*
 *  rec - receive (result, move_no, positions_explored) from the
 *        mailbox mbox.
 */

void mailbox_rec (mailbox *mbox,
		  int *result, int *move_no, int *positions_explored)
{
  multiprocessor_wait (mbox->item_available); //wait for a signal for when there is data in the mailbox to be read
  multiprocessor_wait (mbox->mutex); //wait for a signal for the mailbox to be opened from the child
  *result = mbox->data[mbox->out].result; // These three lines are for the parent to read
  *move_no = mbox->data[mbox->out].move_no; // the data from the mailbox records them by pointing towards their reference
  *positions_explored = mbox->data[mbox->out].positions_explored; 
  mbox->out = (mbox->out + 1) % MAX_MAILBOX_DATA; // Increments the pointer for the next buffer spot by one. If it's 100, the modulus resets it back to 0
  multiprocessor_signal (mbox->mutex); 			// Sends a signal that the mailbox can be opened again to the child
  multiprocessor_signal (mbox->space_available); // Send signals that there is now more space in the mailbox for data
}
}
