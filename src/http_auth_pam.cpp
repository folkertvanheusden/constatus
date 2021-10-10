// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBPAM == 1
#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "http_auth_pam.h"
#include "log.h"

int function_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	pam_response *reply = (pam_response *)appdata_ptr;

	*resp = reply;

	return PAM_SUCCESS;
}

http_auth_pam::http_auth_pam()
{
}

http_auth_pam::~http_auth_pam()
{
}

std::tuple<bool, std::string> http_auth_pam::authenticate(const std::string & username, const std::string & password)
{
	log(LL_INFO, "Authenticate user %s", username.c_str());

	struct pam_response *reply = (struct pam_response *)malloc(sizeof(struct pam_response));
	reply->resp = strdup(password.c_str());
	reply->resp_retcode = 0;

	const struct pam_conv local_conversation { function_conversation, reply };

	pam_handle_t *local_auth_handle = nullptr;
	if (pam_start("common-auth", username.c_str(), &local_conversation, &local_auth_handle) != PAM_SUCCESS) {
		free(reply);
		return std::make_tuple(false, "PAM error");
	}

	int rc = pam_authenticate(local_auth_handle, PAM_SILENT);
	pam_end(local_auth_handle, rc);

	if (rc == PAM_AUTH_ERR)
		return std::make_tuple(false, "Authentication failed");

	if (rc != PAM_SUCCESS)
		return std::make_tuple(false, "PAM error");

	return std::make_tuple(true, "Ok");
}
#endif
