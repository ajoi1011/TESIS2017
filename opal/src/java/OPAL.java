/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 3.0.7
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.opalvoip.opal;

public class OPAL implements OPALConstants {
  public static SWIGTYPE_p_OpalHandleStruct OpalInitialise(long[] INOUT, String INPUT) {
    long cPtr = OPALJNI.OpalInitialise(INOUT, INPUT);
    return (cPtr == 0) ? null : new SWIGTYPE_p_OpalHandleStruct(cPtr, false);
  }

  public static void OpalShutDown(SWIGTYPE_p_OpalHandleStruct IN) {
    OPALJNI.OpalShutDown(SWIGTYPE_p_OpalHandleStruct.getCPtr(IN));
  }

  public static SWIGTYPE_p_OpalMessage OpalGetMessage(SWIGTYPE_p_OpalHandleStruct arg0, long arg1) {
    long cPtr = OPALJNI.OpalGetMessage(SWIGTYPE_p_OpalHandleStruct.getCPtr(arg0), arg1);
    return (cPtr == 0) ? null : new SWIGTYPE_p_OpalMessage(cPtr, false);
  }

  public static SWIGTYPE_p_OpalMessage OpalSendMessage(SWIGTYPE_p_OpalHandleStruct arg0, SWIGTYPE_p_OpalMessage arg1) {
    long cPtr = OPALJNI.OpalSendMessage(SWIGTYPE_p_OpalHandleStruct.getCPtr(arg0), SWIGTYPE_p_OpalMessage.getCPtr(arg1));
    return (cPtr == 0) ? null : new SWIGTYPE_p_OpalMessage(cPtr, false);
  }

  public static void OpalFreeMessage(SWIGTYPE_p_OpalMessage IN) {
    OPALJNI.OpalFreeMessage(SWIGTYPE_p_OpalMessage.getCPtr(IN));
  }

}
