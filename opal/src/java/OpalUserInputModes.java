/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 3.0.7
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.opalvoip.opal;

public final class OpalUserInputModes {
  public final static OpalUserInputModes OpalUserInputDefault = new OpalUserInputModes("OpalUserInputDefault");
  public final static OpalUserInputModes OpalUserInputAsQ931 = new OpalUserInputModes("OpalUserInputAsQ931");
  public final static OpalUserInputModes OpalUserInputAsString = new OpalUserInputModes("OpalUserInputAsString");
  public final static OpalUserInputModes OpalUserInputAsTone = new OpalUserInputModes("OpalUserInputAsTone");
  public final static OpalUserInputModes OpalUserInputAsRFC2833 = new OpalUserInputModes("OpalUserInputAsRFC2833");
  public final static OpalUserInputModes OpalUserInputInBand = new OpalUserInputModes("OpalUserInputInBand");

  public final int swigValue() {
    return swigValue;
  }

  public String toString() {
    return swigName;
  }

  public static OpalUserInputModes swigToEnum(int swigValue) {
    if (swigValue < swigValues.length && swigValue >= 0 && swigValues[swigValue].swigValue == swigValue)
      return swigValues[swigValue];
    for (int i = 0; i < swigValues.length; i++)
      if (swigValues[i].swigValue == swigValue)
        return swigValues[i];
    throw new IllegalArgumentException("No enum " + OpalUserInputModes.class + " with value " + swigValue);
  }

  private OpalUserInputModes(String swigName) {
    this.swigName = swigName;
    this.swigValue = swigNext++;
  }

  private OpalUserInputModes(String swigName, int swigValue) {
    this.swigName = swigName;
    this.swigValue = swigValue;
    swigNext = swigValue+1;
  }

  private OpalUserInputModes(String swigName, OpalUserInputModes swigEnum) {
    this.swigName = swigName;
    this.swigValue = swigEnum.swigValue;
    swigNext = this.swigValue+1;
  }

  private static OpalUserInputModes[] swigValues = { OpalUserInputDefault, OpalUserInputAsQ931, OpalUserInputAsString, OpalUserInputAsTone, OpalUserInputAsRFC2833, OpalUserInputInBand };
  private static int swigNext = 0;
  private final int swigValue;
  private final String swigName;
}

