/*******************************************************************************
 *
 * Module Name: rsutils - Utilities for the resource manager
 *              $Revision: 29 $
 *
 ******************************************************************************/

/*
 *  Copyright (C) 2000 - 2002, R. Byron Moore
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "acpi.h"
#include "acnamesp.h"
#include "acresrc.h"


#define _COMPONENT          ACPI_RESOURCES
	 ACPI_MODULE_NAME    ("rsutils")


/*******************************************************************************
 *
 * FUNCTION:    Acpi_rs_get_prt_method_data
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              Ret_buffer      - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRT value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

acpi_status
acpi_rs_get_prt_method_data (
	acpi_handle             handle,
	acpi_buffer             *ret_buffer)
{
	acpi_operand_object     *ret_obj;
	acpi_status             status;


	ACPI_FUNCTION_TRACE ("Rs_get_prt_method_data");


	/* Parameters guaranteed valid by caller */

	/*
	 *  Execute the method, no parameters
	 */
	status = acpi_ns_evaluate_relative (handle, "_PRT", NULL, &ret_obj);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	if (!ret_obj) {
		/* Return object is required */

		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No object was returned from _PRT\n"));
		return_ACPI_STATUS (AE_TYPE);
	}

	/*
	 * The return object will be a package, so check the parameters.  If the
	 * return object is not a package, then the underlying AML code is corrupt
	 * or improperly written.
	 */
	if (ACPI_TYPE_PACKAGE != ret_obj->common.type) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "_PRT did not return a Package, returned %s\n",
				acpi_ut_get_type_name (ret_obj->common.type)));
		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	/*
	 * Create a resource linked list from the byte stream buffer that comes
	 * back from the _CRS method execution.
	 */
	status = acpi_rs_create_pci_routing_table (ret_obj, ret_buffer);

	/* On exit, we must delete the object returned by Evaluate_object */

cleanup:

	acpi_ut_remove_reference (ret_obj);
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_rs_get_crs_method_data
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              Ret_buffer      - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _CRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

acpi_status
acpi_rs_get_crs_method_data (
	acpi_handle             handle,
	acpi_buffer             *ret_buffer)
{
	acpi_operand_object     *ret_obj;
	acpi_status             status;


	ACPI_FUNCTION_TRACE ("Rs_get_crs_method_data");


	/* Parameters guaranteed valid by caller */

	/*
	 * Execute the method, no parameters
	 */
	status = acpi_ns_evaluate_relative (handle, "_CRS", NULL, &ret_obj);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	if (!ret_obj) {
		/* Return object is required */

		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No object was returned from _CRS\n"));
		return_ACPI_STATUS (AE_TYPE);
	}

	/*
	 * The return object will be a buffer, but check the
	 * parameters.  If the return object is not a buffer,
	 * then the underlying AML code is corrupt or improperly
	 * written.
	 */
	if (ACPI_TYPE_BUFFER != ret_obj->common.type) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "_CRS did not return a Buffer, returned %s\n",
				acpi_ut_get_type_name (ret_obj->common.type)));
		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	/*
	 * Make the call to create a resource linked list from the
	 * byte stream buffer that comes back from the _CRS method
	 * execution.
	 */
	status = acpi_rs_create_resource_list (ret_obj, ret_buffer);

	/* On exit, we must delete the object returned by evaluate_object */

cleanup:

	acpi_ut_remove_reference (ret_obj);
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_rs_get_prs_method_data
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              Ret_buffer      - a pointer to a buffer structure for the
 *                                  results
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the _PRS value of an object
 *              contained in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

acpi_status
acpi_rs_get_prs_method_data (
	acpi_handle             handle,
	acpi_buffer             *ret_buffer)
{
	acpi_operand_object     *ret_obj;
	acpi_status             status;


	ACPI_FUNCTION_TRACE ("Rs_get_prs_method_data");


	/* Parameters guaranteed valid by caller */

	/*
	 * Execute the method, no parameters
	 */
	status = acpi_ns_evaluate_relative (handle, "_PRS", NULL, &ret_obj);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	if (!ret_obj) {
		/* Return object is required */

		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No object was returned from _PRS\n"));
		return_ACPI_STATUS (AE_TYPE);
	}

	/*
	 * The return object will be a buffer, but check the
	 * parameters.  If the return object is not a buffer,
	 * then the underlying AML code is corrupt or improperly
	 * written..
	 */
	if (ACPI_TYPE_BUFFER != ret_obj->common.type) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "_PRS did not return a Buffer, returned %s\n",
				acpi_ut_get_type_name (ret_obj->common.type)));
		status = AE_AML_OPERAND_TYPE;
		goto cleanup;
	}

	/*
	 * Make the call to create a resource linked list from the
	 * byte stream buffer that comes back from the _CRS method
	 * execution.
	 */
	status = acpi_rs_create_resource_list (ret_obj, ret_buffer);

	/* On exit, we must delete the object returned by evaluate_object */

cleanup:

	acpi_ut_remove_reference (ret_obj);
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_rs_set_srs_method_data
 *
 * PARAMETERS:  Handle          - a handle to the containing object
 *              In_buffer       - a pointer to a buffer structure of the
 *                                  parameter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to set the _SRS of an object contained
 *              in an object specified by the handle passed in
 *
 *              If the function fails an appropriate status will be returned
 *              and the contents of the callers buffer is undefined.
 *
 ******************************************************************************/

acpi_status
acpi_rs_set_srs_method_data (
	acpi_handle             handle,
	acpi_buffer             *in_buffer)
{
	acpi_operand_object     *params[2];
	acpi_status             status;
	acpi_buffer             buffer;


	ACPI_FUNCTION_TRACE ("Rs_set_srs_method_data");


	/* Parameters guaranteed valid by caller */

	/*
	 * The In_buffer parameter will point to a linked list of
	 * resource parameters.  It needs to be formatted into a
	 * byte stream to be sent in as an input parameter to _SRS
	 *
	 * Convert the linked list into a byte stream
	 */
	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;
	status = acpi_rs_create_byte_stream (in_buffer->pointer, &buffer);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	/*
	 * Init the param object
	 */
	params[0] = acpi_ut_create_internal_object (ACPI_TYPE_BUFFER);
	if (!params[0]) {
		acpi_os_free (buffer.pointer);
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	/*
	 * Set up the parameter object
	 */
	params[0]->buffer.length  = buffer.length;
	params[0]->buffer.pointer = buffer.pointer;
	params[1] = NULL;

	/*
	 * Execute the method, no return value
	 */
	status = acpi_ns_evaluate_relative (handle, "_SRS", params, NULL);

	/*
	 * Clean up and return the status from Acpi_ns_evaluate_relative
	 */
	acpi_ut_remove_reference (params[0]);
	return_ACPI_STATUS (status);
}

