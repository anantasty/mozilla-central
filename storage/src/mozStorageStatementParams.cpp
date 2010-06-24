/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMemory.h"
#include "nsString.h"

#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementParams.h"
#include "mozIStorageStatement.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// StatementParams

StatementParams::StatementParams(mozIStorageStatement *aStatement) :
    mStatement(aStatement)
{
  NS_ASSERTION(mStatement != nsnull, "mStatement is null");
  (void)mStatement->GetParameterCount(&mParamCount);
}

NS_IMPL_ISUPPORTS2(
  StatementParams,
  mozIStorageStatementParams,
  nsIXPCScriptable
)

////////////////////////////////////////////////////////////////////////////////
//// nsIXPCScriptable

#define XPC_MAP_CLASSNAME StatementParams
#define XPC_MAP_QUOTED_CLASSNAME "StatementParams"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h"

NS_IMETHODIMP
StatementParams::SetProperty(nsIXPConnectWrappedNative *aWrapper,
                             JSContext *aCtx,
                             JSObject *aScopeObj,
                             jsid aId,
                             jsval *_vp,
                             PRBool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

  if (JSID_IS_INT(aId)) {
    int idx = JSID_TO_INT(aId);

    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByIndex(idx, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (JSID_IS_STRING(aId)) {
    JSString *str = JSID_TO_STRING(aId);
    NS_ConvertUTF16toUTF8 name(reinterpret_cast<const PRUnichar *>
                                   (::JS_GetStringChars(str)),
                               ::JS_GetStringLength(str));

    // check to see if there's a parameter with this name
    nsCOMPtr<nsIVariant> variant(convertJSValToVariant(aCtx, *_vp));
    NS_ENSURE_TRUE(variant, NS_ERROR_UNEXPECTED);
    nsresult rv = mStatement->BindByName(name, variant);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
StatementParams::NewEnumerate(nsIXPConnectWrappedNative *aWrapper,
                              JSContext *aCtx,
                              JSObject *aScopeObj,
                              PRUint32 aEnumOp,
                              jsval *_statep,
                              jsid *_idp,
                              PRBool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);

  switch (aEnumOp) {
    case JSENUMERATE_INIT:
    {
      // Start our internal index at zero.
      *_statep = JSVAL_ZERO;

      // And set our length, if needed.
      if (_idp)
        *_idp = INT_TO_JSID(mParamCount);

      break;
    }
    case JSENUMERATE_NEXT:
    {
      NS_ASSERTION(*_statep != JSVAL_NULL, "Internal state is null!");

      // Make sure we are in range first.
      PRUint32 index = static_cast<PRUint32>(JSVAL_TO_INT(*_statep));
      if (index >= mParamCount) {
        *_statep = JSVAL_NULL;
        return NS_OK;
      }

      // Get the name of our parameter.
      nsCAutoString name;
      nsresult rv = mStatement->GetParameterName(index, name);
      NS_ENSURE_SUCCESS(rv, rv);

      // But drop the first character, which is going to be a ':'.
      JSString *jsname = ::JS_NewStringCopyN(aCtx, &(name.get()[1]),
                                             name.Length() - 1);
      NS_ENSURE_TRUE(jsname, NS_ERROR_OUT_OF_MEMORY);

      // Set our name.
      if (!::JS_ValueToId(aCtx, STRING_TO_JSVAL(jsname), _idp)) {
        *_retval = PR_FALSE;
        return NS_OK;
      }

      // And increment our index.
      *_statep = INT_TO_JSVAL(++index);

      break;
    }
    case JSENUMERATE_DESTROY:
    {
      // Clear our state.
      *_statep = JSVAL_NULL;

      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
StatementParams::NewResolve(nsIXPConnectWrappedNative *aWrapper,
                            JSContext *aCtx,
                            JSObject *aScopeObj,
                            jsid aId,
                            PRUint32 aFlags,
                            JSObject **_objp,
                            PRBool *_retval)
{
  NS_ENSURE_TRUE(mStatement, NS_ERROR_NOT_INITIALIZED);
  // We do not throw at any point after this unless our index is out of range
  // because we want to allow the prototype chain to be checked for the
  // property.

  bool resolved = false;
  PRBool ok = PR_TRUE;
  if (JSID_IS_INT(aId)) {
    PRUint32 idx = JSID_TO_INT(aId);

    // Ensure that our index is within range.  We do not care about the
    // prototype chain being checked here.
    if (idx >= mParamCount)
      return NS_ERROR_INVALID_ARG;

    ok = ::JS_DefineElement(aCtx, aScopeObj, idx, JSVAL_VOID, nsnull,
                            nsnull, 0);
    resolved = true;
  }
  else if (JSID_IS_STRING(aId)) {
    JSString *str = JSID_TO_STRING(aId);
    jschar *nameChars = ::JS_GetStringChars(str);
    size_t nameLength = ::JS_GetStringLength(str);

    // Check to see if there's a parameter with this name, and if not, let
    // the rest of the prototype chain be checked.
    NS_ConvertUTF16toUTF8 name(reinterpret_cast<const PRUnichar *>(nameChars),
                               nameLength);
    PRUint32 idx;
    nsresult rv = mStatement->GetParameterIndex(name, &idx);
    if (NS_SUCCEEDED(rv)) {
      ok = ::JS_DefineUCProperty(aCtx, aScopeObj, nameChars, nameLength,
                                 JSVAL_VOID, nsnull, nsnull, 0);
      resolved = true;
    }
  }

  *_retval = ok;
  *_objp = resolved && ok ? aScopeObj : nsnull;
  return NS_OK;
}

} // namespace storage
} // namespace mozilla
