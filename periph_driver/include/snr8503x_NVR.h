/*******************************************************************************
 * snr8503x_NVR.h
 *
 * Originally declared Read_Trim() and Prog_Trim() — the only entry points to
 * the chip's factory NVR (non-volatile register / OTP) calibration memory.
 * Both functions lived in the proprietary snr8503x_nvr.lib that this project
 * no longer links.
 *
 * The only consumer was snr8503x_dac.c (DAC trim load). That file has been
 * patched to use identity-default calibration values directly, so no NVR
 * access is needed anywhere in the firmware. See the README's
 * "Closed-library replacement" section for details.
 *
 * Original LINKO/SNANER copyright on this file: BSD-3-Clause via the LINKO
 * platform_software release.
 *******************************************************************************/

#ifndef SNR8503X_NVR_H
#define SNR8503X_NVR_H

/* No declarations — Read_Trim and Prog_Trim are intentionally absent.
 * Anything that tries to call them will fail at link time, which is the
 * desired behaviour: a missing-symbol error is a clearer signal than a
 * silent runtime-fail stub. */

#endif /* SNR8503X_NVR_H */
