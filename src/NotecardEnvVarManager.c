#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "note-c/note.h"

#include "NotecardEnvVarManager.h"

struct NotecardEnvVarManager {
    envVarCb userCb;
    void *userCtx;
};

/**
 * Internal function to create a request for the specified environment variables
 * to send to the Notecard. This function does NOT send the request to the
 * Notecard.
 *
 * @param vars    Pointer to an array of C-strings of variables to fetch.
 * @param numVars The number of variable strings in vars. If set to the special
 *                value NEVM_ENV_VAR_ALL, the request will be for all
 *                environment variables, regardless of what is specified by
 *                vars.
 *
 * @return Valid request J * on success and NULL on failure.
 */
static J *_buildEnvGetRequest(const char **vars, size_t numVars)
{
    if (vars == NULL && numVars != NEVM_ENV_VAR_ALL) {
        NOTE_C_LOG_ERROR("vars must be non-NULL unless numVars is "
                         "NEVM_ENV_VAR_ALL.\r\n");
        return NULL;
    }

    bool err = false;
    J *req = NoteNewRequest("env.get");
    if (req == NULL) {
        err = true;
        NOTE_C_LOG_ERROR("Out of memory.\r\n");
    } else if (numVars != NEVM_ENV_VAR_ALL) {
        J *names = JCreateArray();
        if (names == NULL) {
            err = true;
            NOTE_C_LOG_ERROR("Out of memory.\r\n");
        } else {
            JAddItemToObject(req, "names", names);
            for (size_t i = 0; i < numVars; ++i) {
                J *varStr = JCreateStringReference(vars[i]);
                if (varStr != NULL) {
                    JAddItemToArray(names, varStr);
                } else {
                    err = true;
                    NOTE_C_LOG_ERROR("Out of memory.\r\n");
                    break;
                }
            }
        }
    }

    if (err) {
        JDelete(req);
        req = NULL;
    }

    return req;
}

/**
 * Fetch environment variables from the Notecard, calling the user-provided
 * callback on each variable:value pair.
 *
 * @param man     Pointer to a NotecardEnvVarManager object.
 * @param vars    Pointer to an array of C-strings of variables to fetch.
 * @param numVars The number of variable strings in vars. If set to the special
 *                value NEVM_ENV_VAR_ALL, all environment variables will be
 *                fetched, regardless of what is specified by vars.
 *
 * @return NEVM_SUCCESS on success and NEVM_FAILURE on failure.
 */
int NotecardEnvVarManager_fetch(NotecardEnvVarManager *man, const char **vars,
                                size_t numVars)
{
    if (man == NULL) {
        NOTE_C_LOG_ERROR("NULL manager.\r\n");
        return NEVM_FAILURE;
    }
    if (man->userCb == NULL) {
        NOTE_C_LOG_INFO("No user callback set. No variables will be fetched."
                        "\r\n");
        return NEVM_SUCCESS;
    }

    int ret = NEVM_SUCCESS;
    J *rsp = NoteRequestResponse(_buildEnvGetRequest(vars, numVars));
    if (rsp != NULL) {
        if (!NoteResponseError(rsp)) {
            J *body = JGetObject(rsp, "body");
            if (body != NULL) {
                J *item = NULL;
                JObjectForEach(item, body) {
                    char *var = item->string;
                    char *val = JGetStringValue(item);

                    // Call the user's callback on each variable:value pair
                    // in the response.
                    man->userCb(var, val, man->userCtx);
                }
            } else {
                NOTE_C_LOG_ERROR("No \"body\" field in env.get response.\r\n");
                ret = NEVM_FAILURE;
            }
        } else {
            NOTE_C_LOG_ERROR("Error in env.get response.\r\n");
            ret = NEVM_FAILURE;
        }
    } else {
        NOTE_C_LOG_ERROR("NULL response to env.get request.\r\n");
        ret = NEVM_FAILURE;
    }

    NoteDeleteResponse(rsp);

    return ret;
}

/**
 * Free a NotecardEnvVarManager's memory.
 *
 * @param man Pointer to a NotecardEnvVarManager.
 */
void NotecardEnvVarManager_free(NotecardEnvVarManager *man)
{
    NoteFree(man);
}

/**
 * Create a new NotecardEnvVarManager.
 *
 * @return A valid pointer to a NotecardEnvVarManager on success and NULL on
 *         failure.
 */
NotecardEnvVarManager *NotecardEnvVarManager_alloc(void)
{
    NotecardEnvVarManager *man = (NotecardEnvVarManager *)NoteMalloc(
                                     sizeof(NotecardEnvVarManager));
    if (man != NULL) {
        memset(man, 0, sizeof(*man));
        NOTE_C_LOG_INFO("Successfully allocated NotecardEnvVarManager.\r\n");
    }

    return man;
}

/**
 * Set the callback that the manager will call on every variable:value pair
 * fetched from the Notecard.
 *
 * @param man     Pointer to a NotecardEnvVarManager object.
 * @param userCb  The callback.
 * @param userCtx Pointer to a user context. This pointer will be passed in
 *                to the userCb whenever it's called, allowing the user to
 *                provide arbitrary data to the callback.
 *
 * @return NEVM_SUCCESS on success and NEVM_FAILURE on failure.
 */
int NotecardEnvVarManager_setEnvVarCb(NotecardEnvVarManager *man,
                                      envVarCb userCb, void *userCtx)
{
    if (man == NULL) {
        NOTE_C_LOG_ERROR("NULL manager.\r\n");
        return NEVM_FAILURE;
    }

    man->userCb = userCb;
    man->userCtx = userCtx;

    return NEVM_SUCCESS;
}
