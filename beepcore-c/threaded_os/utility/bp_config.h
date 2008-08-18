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
 * $Id: bp_config.h,v 1.9 2002/01/01 00:25:15 mrose Exp $
 *
 * IW_config.h
 *
 * Functionality for parsing the configuration file.
 *
 * Values stored are that of the CDATA of the leaf elements.
 *
 * 
 * Note that we expect the config file to be well formed XML, and 
 * that we ignore anything contained in a <comment> element and
 * also ignore all attribute values.  Attributes may thus be used as
 * comments as well.  Nested <comment> elements are not supported.
 *
 */

#ifndef __IW_CONFIG_H__
#define __IW_CONFIG_H__ 1

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name Configuration functions
 *
 * \Label{return-codes} Return codes for
 * {@link config_parse_file config_parse_file},
 * {@link config_parse config_parse},
 * {@link config_set config_set},
 * {@link config_test_and_set config_test_and_set}, and
 * {@link config_delete config_delete}:
 * <ul>
 * <li>CONFIG_OK</li>
 * <li>CONFIG_PARSE_OK</li>
 * <li>CONFIG_NOT_INITIALIZED</li>
 * <li>CONFIG_ALLOC_FAIL</li>
 * <li>CONFIG_PARSE_ERROR</li>
 * <li>CONFIG_FILE_ERROR</li>
 * <li>CONFIG_ALREADY_SET</li>
 * </ul>
 */
//@{


#define CONFIG_OK                        0
#define CONFIG_PARSE_OK                  0
#define CONFIG_NOT_INITIALIZED          -1
#define CONFIG_ALLOC_FAIL               -2
#define CONFIG_PARSE_ERROR              -3
#define CONFIG_FILE_ERROR               -4
#define CONFIG_ALREADY_SET               1

#define CONFIG_KEY_SEPARATOR              ' '

struct configobj;
struct cfgsearchobj;

/**
 * Allocates a configuration structure.
 * <p>
 * Use {@link config_destroy config_destroy} to destroy the structure returned.
 *
 * @param defaults A pointer to the configuration structure to clone,
 *	           or <b>NULL</b>
 *
 * @return A pointer to an initialized configuration structure, or <b>NULL</b>.
 */
extern struct configobj* config_new(struct configobj *defaults);

/**
 * Deallocates a configuration structure returned by
 * {@link config_new config_new}.
 *
 * @param appconfig A pointer to the configuration structure.
 */
extern void config_destroy(struct configobj *appconfig);

/**
 * Parse a file into a configuration structure.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param filename The file to parse.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern int config_parse_file(struct configobj *appconfig,
			     char *filename);

/**
 * Parse a string into a configuration structure.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param buffer A character pointer.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern int config_parse(struct configobj *appconfig,
			char *buffer);

/**
 * Use a configuration structure to map a key to a value.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * configuration structure is destroyed.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param key The key to search for.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern char* config_get(struct configobj *appconfig,
			char *key);

/**
 * Use a key to store a value in a configuration structure.
 * <p>
 * This function makes copies of the <i>key</i> and <i>value</i>
 * parameters, so the caller may free them upon return.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param key The key to store under.
 *
 * @param value The value to store.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern int config_set(struct configobj *appconfig,
		      char *key,
		      char *value);

/**
 * Use a configuration structure to map a key to a value,
 * but only if that key isn't present.
 * <p>
 * This function makes copies of the <i>key</i> and <i>value</i>
 * parameters, so the caller may free them upon return.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param key The key to search for.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern int config_test_and_set(struct configobj *appconfig,
			       char *key,
			       char *value);

/**
 * Use a configuration structure to map a key to a value.
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param key The key to delete.
 *
 * @return \URL[return code]{Configurationfunctions.html#return-codes}.
 */
extern int config_delete(struct configobj *appconfig,
			 char *key);


/**
 * Initialize an object to search a configuration structure.
 * <p>
 * Use {@link config_search_fin config_search_fin} to destroy the structure
 * returned. 
 *
 * @param appconfig A pointer to the configuration structure.
 *
 * @param prefix String that all returned keys must start with
 *	(or <b>NULL</b>).
 *
 * @param substring String that all returned keys must contain
 *	(or <b>NULL</b>).
 *
 * @return A pointer to an initialized search structure, or <b>NULL</b>.
 */
extern struct cfgsearchobj* config_search_init(struct configobj *appconfig,
					       char *prefix,
					       char *substring);

/**
 * Resets a search object returned by
 * {@link config_search_init config_search_init}.
 *
 * @param csearch A pointer to the search structure.
 */
extern struct cfgsearchobj* config_search_rewind(struct cfgsearchobj *csearch);

/**
 * Deallocates a search  structure returned by
 * {@link config_search_init config_search_init}.
 *
 * @param csearch A pointer to the search structure.
 */
extern int config_search_fin(struct cfgsearchobj **csearch);

/**
 * Returns the first key matching the search.
 *
 * @param csearch A pointer to the search structure.
 */
extern char *config_search_string_firstkey(struct cfgsearchobj *csearch);

/**
 * Returns the next key matching the search.
 *
 * @param csearch A pointer to the search structure.
 */
extern char *config_search_string_nextkey(struct cfgsearchobj *csearch);

/**
 * Returns the value associated with the key returned by the last match.
 *
 * @param csearch A pointer to the search structure.
 */
extern char *config_search_value(struct cfgsearchobj *csearch);

//@}

#if defined(__cplusplus)
}
#endif

#endif
