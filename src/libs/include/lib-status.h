#pragma once
 
typedef enum Status
{
    STATUS_SUCCESS,
    STATUS_FAILURE,
    STATUS_INVALID_NAME,
    STATUS_INVALID_PATH,
    STATUS_ALREADY_EXISTS,
    STATUS_NOT_ALLOWED,
    STATUS_END_OF_FILE,
    STATUS_CORRUPT,
    STATUS_INVALID_POINTER,
} Status;

static const char* statusToString[] =
{
    [STATUS_SUCCESS] = "SUCCESS",
    [STATUS_FAILURE] = "FAILURE",
    [STATUS_INVALID_NAME] = "INVALID_NAME",
    [STATUS_INVALID_PATH] = "INVALID_PATH",
    [STATUS_ALREADY_EXISTS] = "ALREADY_EXISTS",
    [STATUS_NOT_ALLOWED] = "NOT_ALLOWED",
    [STATUS_END_OF_FILE] = "END_OF_FILE",
    [STATUS_CORRUPT] = "CORRUPT",
    [STATUS_INVALID_POINTER] = "INVALID_POINTER"
};

#if defined(__cplusplus)
extern "C" {
#endif

Status status();

const char* status_string();

#if defined(__cplusplus)
} /* extern "C" */
#endif
