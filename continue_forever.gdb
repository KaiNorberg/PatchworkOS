define bt_and_continue_forever
  # Turn off pagination so GDB doesn't prompt you to press enter
  set pagination off

  # Loop indefinitely
  while (1)
    # Print the backtrace
    bt

    # Continue execution. This will run until the next breakpoint,
    # signal, or program exit.
    continue

    # Note: If you don't have any breakpoints set, 'continue' will
    # just run the program to completion or until a signal is received.
    # If you want to see a backtrace at *every* instruction/line,
    # this approach is generally not what you want. You'd typically
    # use this with a breakpoint that periodically gets hit.
  end
end

document bt_and_continue_forever
  Repeatedly prints a backtrace and then continues execution.
  Useful for observing stack traces over time when breakpoints are hit.
  Use Ctrl-C to interrupt the loop.
end