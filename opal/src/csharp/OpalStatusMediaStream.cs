//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (http://www.swig.org).
// Version 4.0.1
//
// Do not make changes to this file unless you know what you are doing--modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------


public class OpalStatusMediaStream : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal OpalStatusMediaStream(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(OpalStatusMediaStream obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  ~OpalStatusMediaStream() {
    Dispose(false);
  }

  public void Dispose() {
    Dispose(true);
    global::System.GC.SuppressFinalize(this);
  }

  protected virtual void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          OPALPINVOKE.delete_OpalStatusMediaStream(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public string callToken {
    set {
      OPALPINVOKE.OpalStatusMediaStream_callToken_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusMediaStream_callToken_get(swigCPtr);
      return ret;
    } 
  }

  public string identifier {
    set {
      OPALPINVOKE.OpalStatusMediaStream_identifier_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusMediaStream_identifier_get(swigCPtr);
      return ret;
    } 
  }

  public string type {
    set {
      OPALPINVOKE.OpalStatusMediaStream_type_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusMediaStream_type_get(swigCPtr);
      return ret;
    } 
  }

  public string format {
    set {
      OPALPINVOKE.OpalStatusMediaStream_format_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusMediaStream_format_get(swigCPtr);
      return ret;
    } 
  }

  public OpalMediaStates state {
    set {
      OPALPINVOKE.OpalStatusMediaStream_state_set(swigCPtr, (int)value);
    } 
    get {
      OpalMediaStates ret = (OpalMediaStates)OPALPINVOKE.OpalStatusMediaStream_state_get(swigCPtr);
      return ret;
    } 
  }

  public int volume {
    set {
      OPALPINVOKE.OpalStatusMediaStream_volume_set(swigCPtr, value);
    } 
    get {
      int ret = OPALPINVOKE.OpalStatusMediaStream_volume_get(swigCPtr);
      return ret;
    } 
  }

  public string watermark {
    set {
      OPALPINVOKE.OpalStatusMediaStream_watermark_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusMediaStream_watermark_get(swigCPtr);
      return ret;
    } 
  }

  public OpalStatusMediaStream() : this(OPALPINVOKE.new_OpalStatusMediaStream(), true) {
  }

}
