#include "main.h"

void BeginPage(
  PStringStream & html, 
  PStringStream & pmeta, 
  const char    * ptitle, 
  const char    * title, 
  const char    * quotekey
);
  
void EndPage (PStringStream & html, PString copyr);

class HomePage : public PServiceHTTPString                               
{                                                                        
  public:                                                                
    HomePage(MyProcess & app, PHTTPAuthority & auth);                    
                                                                         
  protected:                                                             
    MyProcess & app;                                                     
};   


class JpegFrameHTTP : public PServiceHTTPString
{
  public:
    JpegFrameHTTP(MyProcess & app, PHTTPAuthority & auth); 
    PBoolean OnGET (                                                     
      PHTTPServer & server,                                              
      const PHTTPConnectionInfo & connectInfo                            
    );  
    PMutex mutex;
  private:
    MyProcess & app;
};

class ControlRoomPage : public PServiceHTTPString                        
{                                                                        
  public:                                                                
    ControlRoomPage(MyProcess & app, PHTTPAuthority & auth);             
                                                                         
    PBoolean OnGET (                                                     
      PHTTPServer & server,                                              
      const PHTTPConnectionInfo & connectInfo                            
    );   
    
    PBoolean Post(
      PHTTPRequest & request,       
      const PStringToString & data, 
      PHTML & replyMessage          
    );                                                                
                                                                         
  protected:                                                             
    MyProcess & app;                                                     
};                                                                      

class InteractiveHTTP : public PServiceHTTPString
{
  public:
    InteractiveHTTP(MyProcess & app, PHTTPAuthority & auth);
    
    PBoolean OnGET (                                                     
      PHTTPServer & server,                                              
      const PHTTPConnectionInfo & connectInfo                            
    );
    
    protected:  
    MyProcess & app;
};
