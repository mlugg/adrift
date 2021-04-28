# adrift protocol

Connections are initiated over a socket, either UNIX or TCP. Messages are
terminated with an ASCII line feed ('\n', 0x0A). Arguments are separated with an
ASCII SPACE (' ', 0x20). Human-readable names are an exception to the usual
system; they are always the last argument of a message, and may contain
whitespace (excluding LF).

## Negotiation

	C2S
		HELLO a b ...                Inform the server we support protocol versions 'a', 'b' etc.

	S2C
		HELLO v                      Confirm to the client we are ready to receive data on protocol version 'v'.


## Protocol

The following describes **protocol version 0.1**.

### General

	C2S
		GAME <name> <hr>             Update the name and human-readable name of this game. NOTE: 'name' may not contain whitespace.
		CAT <name> <hr>              Update the active category and its human-readable name. NOTE: 'name' may not contain whitespace.

### Timer

	C2S
		START <us>                   Start the timer at 'us' microseconds. If the timer is already running, acts like a RESET immediately followed by a START.
		SYNC <us>                    Update the timer to 'us' microseconds. Effective regardless of whether the timer is already running.
		SPLIT <us>                   Perform a split on the current run at 'us' microseconds. If this is the last split, finish the run at 'us' microseconds. Only effective if a run is active.
		RESET <us>                   Reset the current run, setting the timer back to 'us' microseconds and stopping it.

### Split communication

If the client will tell the server about splits, it sends these packets whenever
the splits change. After a CAT message, all data must be re-sent. If a split is
renamed, only its SPLITNAME must be sent. An NSPLITS message invalidates all
previous split names, which must be resent.

	C2S
		NSPLITS <n>                  Inform the server there are 'n' splits in the active category.
		SPLITNAME <n> <hr>           Inform the server that split 'n' has the given human-readable name.

### Recovery

	C2S
		RECOVERDATA <data>           Update the client's custom recovery data. 'data' may be empty.
		RECOVERAPPEND <data>         Append data to the end of the client's custom recovery data.

	S2C
		RECOVER <catname> <n> <us> <data>    Recovery info: inform the client that we were using category 'catname' with 'n' splits, that the timer was currently on 'us' microseconds, and that its custom recovery data was 'data'.
