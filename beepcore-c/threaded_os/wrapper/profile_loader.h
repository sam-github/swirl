/*
 * Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the Blocks Public License (the
 * "License"); You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at http://www.beepcore.org/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 */
/*
 * $Id: profile_loader.h,v 1.4 2001/11/03 17:10:15 mrose Exp $
 *
 * profile_loader.h
 *
 * Utilities to load profile libraries at runtime.
 *
 */

#include "bp_wrapper.h"
#include "../utility/bp_config.h"

/**
 * Dynamically loads profile-registration structures using a configuration
 * object and invoking the {@link (*profile_shlib_init) entry-point} for
 * each profile module identified.
 *
 * @param appconfig A configuration object which has the proper values set
 *                  defining the profiles to be loaded. For an example,
 *                  see <code>threaded_os/examples/beepd.cfg</code>
 *
 * @param pgmname Typically argv[0].
 *
 * @param dataname The application configuration, usually "default".
 *
 * @return A pointer to a linked list of profile-registration structures.
 **/
extern PROFILE_REGISTRATION* load_beep_profiles(struct configobj *appconfig,
						char *pgmname,
						char *dataname);
