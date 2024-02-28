[bits 64]

;Janky but it avoids code duplication
%define .text .program_loader
%include "asym/functions/exit.s"
%include "asym/functions/status.s"
%include "asym/functions/open.s"
%include "asym/functions/close.s"
%include "asym/functions/read.s"
%include "asym/functions/seek.s"
%include "asym/functions/sys_test.s"
