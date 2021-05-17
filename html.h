/*
 * html.h
 * 
*/

/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                       C L A S E S  H T M L                            */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/

#ifndef _HTML_H
#define _HTML_H

#include "main.h"

void BeginPage(PStringStream & html, PStringStream & pmeta, 
                                     const char    * ptitle, 
                                     const char    * title, 
                                     const char    * quotekey);
  
void EndPage(PStringStream & html, PString copyr);

/*************************************************************************/
/*                                                                       */
/* <clase BaseStatusPage>                                                */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de PServiceHTTPString que describe un post HTTP      */
/*  con información detallada de estados, bases de datos, registros en   */
/*  el servidor HTTP.                                                    */
/*                                                                       */
/*************************************************************************/
class BaseStatusPage : public PServiceHTTPString
{
  PCLASSINFO(BaseStatusPage, PServiceHTTPString);
  public:
    BaseStatusPage(
      Manager & mgr,
      const PHTTPAuthority & auth,
      const char * name
    );

    virtual PString LoadText(
      PHTTPRequest & request 
    );
    virtual PBoolean Post(
      PHTTPRequest & request,
      const PStringToString &,
      PHTML & msg
    );
    Manager & GetManager() { return m_manager; }

  protected: 
    virtual void CreateContent( 
      PHTML & html,
      const PStringToString & query 
    ) const = 0;
    
    virtual void CreateHTML(
      PHTML & html_page,
      const PStringToString & query
    );
    
    virtual const char * GetTitle() const = 0;
    virtual bool OnPostControl(const PStringToString & /*data*/,
                               PHTML & /*msg*/)
    {
      return false;
    }

    Manager            & m_manager;
    unsigned               m_refreshRate;
    //MCUDetailRecord   * m_cdr;
};


/****************************************************************************************/
/*                                                                                      */
/* <clase CallStatusPage>                                                               */
/*                                                                                      */
/* <Descripción>                                                                        */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info de las         */
/* llamadas OPAL entrantes/salientes.                                                   */
/*                                                                                      */
/****************************************************************************************/
class CallStatusPage : public BaseStatusPage                             
{                                                                        
    PCLASSINFO(CallStatusPage, BaseStatusPage);                          
  public:                                                                
    CallStatusPage(Manager & mgr, const PHTTPAuthority & auth);        
                                                                         
    void GetCalls(PArray<PString> & copy) const;                         
    PINDEX GetCallCount() const { return m_calls.GetSize(); }            
                                                                         
  protected:                                                             
    virtual void CreateContent(PHTML & html, const PStringToString & query) const;                                                             
    virtual const char * GetTitle() const;                               
    virtual PString LoadText(PHTTPRequest & request);                    
    virtual bool OnPostControl(const PStringToString & data,PHTML & msg);                                                                   
    PArray<PString> m_calls;                                             
    PDECLARE_MUTEX(m_mutex);                                             
};                                                                       

/*************************************************************************/
/*                                                                       */
/* <clase CDRListPage>                                                   */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  resumida del CDR (Call Detail Record).                               */
/*                                                                       */
/*************************************************************************/
class CDRListPage : public BaseStatusPage                                
{                                                                        
  PCLASSINFO(CDRListPage, BaseStatusPage);                               
  public:                                                                
    CDRListPage(Manager & mgr, const PHTTPAuthority & auth);           
                                                                         
  protected:                                                             
    virtual void CreateContent(                                          
      PHTML & html,                                                      
      const PStringToString & query                                      
    ) const;                                                             
    virtual const char * GetTitle() const;                               
};                                                                       
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <clase CDRPage>                                                       */
/*                                                                       */
/* <Descripción>                                                         */
/*   Clase derivada de BaseStatusPage que describe un post HTTP con info */
/*  detallada del CDR (Call Detail Record).                              */
/*                                                                       */
/*************************************************************************/
class CDRPage : public BaseStatusPage                                    
{                                                                        
  PCLASSINFO(CDRPage, BaseStatusPage);                                   
  public:                                                                
    CDRPage(Manager & mgr, const PHTTPAuthority & auth);               
                                                                         
  protected:                                                             
    virtual void CreateContent(                                          
      PHTML & html,                                                      
      const PStringToString & query                                      
    ) const;                                                             
    virtual const char * GetTitle() const;                               
};                                                                       
/*************************************************************************/

class TablePConfigPage : public PConfigPage
{
  public:
   TablePConfigPage(PHTTPServiceProcess & app, const PString & title, 
                                               const PString & section, 
                                               const PHTTPAuthority & auth);
   
   PString SeparatorField(PString name="")
   {
     PString s = "<tr><td align='left' colspan='3' style='background-color:white;padding:0px;'><p style='text-align:center;"+textStyle+"'><b>"+name+"</b></p></td>";
     return s;
   }

   PString StringField(PString name, PString value, int sizeInput=12, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + StringItem(name, value, sizeInput) + InfoItem(info, rowSpan);
   }
   
   PString StringFieldUrl(PString name, PString value, int sizeInput=12, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + StringItemUrl(name, value, sizeInput) + InfoItem(info, rowSpan);
   }
   
   PString IntegerField(PString name, int value, int min=-2147483647, int max=2147483647, int sizeInput=15, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + IntegerItem(name, value, min, max) + InfoItem(info, rowSpan);
   }

   PString BoolField(PString name, bool value, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + BoolItem(name, value) + InfoItem(info, rowSpan);
   }
   
   PString RadioField(PString name, bool value, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + RadioItem(name, value) + InfoItem(info, rowSpan);
   }

   PString SelectField(PString name, PString value, PString values, int width=120, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     return NewRowText(name) + SelectItem(name, value, values, width) + InfoItem(info, rowSpan);
   }

   PString ArrayField(PString name, PString values, int size=12, PString info="", int readonly=false, PINDEX rowSpan=1)
   {
     PStringArray data = values.Tokenise(separator);
     
     PString s = NewRowText(name);
     s += NewItemArray(name);
     if(data.GetSize() == 0)
     {
       s += StringItemArray(name, "", size);
     } else {
       for(PINDEX i = 0; i < data.GetSize(); i++) 
         s += StringItemArray(name, data[i], size);
     }
     s += EndItemArray();
     s += InfoItem(info, rowSpan);
     return s;
   }

   PString NewRowColumn(PString name, int width=250)
   {
     return "<tr style='padding:0px;margin:0px;'>"+colStyle+"width:"+PString(width)+"px'><p style='"+textStyle+";width:"+PString(width)+"px'>"+name+"</p>";
   }

   PString NewRowText(PString name)
   {
     PString s = "<tr style='padding:0px;margin:0px;'>"+rowStyle+"<input name='"+name+"' value='"+name+"' type='hidden'><p style='"+textStyle+"'>"+name+"</p>";
     s += "</td>";
     if(buttons() != "") s += rowStyle+buttons()+"</td>";
     return s;
   }

   PString NewRowInput(PString name, int size=15, int readonly=false)
   {
     PString value = name;
     if(name == "empty") value = "";
     PString s = "<tr style='padding:0px;margin:0px;'>"+rowStyle+"<input type='text' name='"+name+"' size='"+PString(size)+"' value='"+value+"' style='"+inputStyle+"'";
     if(!readonly) s += "></input>"; else s += "readonly></input>";
     if(!readonly) s += buttons();
     s += "</td>";
     return s;
   }

   PString EndRow() { return "</tr>"; }

   PString ColumnItem(PString name, int width=120)
   {
     return colStyle+"width:"+PString(width)+"px'><p style='"+textStyle+"'>"+name+"</p>";
   }

   PString InfoItem(PString name, PINDEX rowSpan=1)
   {
     PString s;
     if(rowSpan>0)
     {
       s = itemInfoStyle;
       if(rowSpan == 1) s.Replace("rowspan='%ROWSPAN%' valign='top'","",true,0);
       else s.Replace("%ROWSPAN%",PString(rowSpan),true,0);
       s += "<p style='"+textStyle+"'>"+name+"</p></td>";
     }
     return s;
   }

   PString StringItem(PString name, PString value, int size=12, int readonly=false)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += itemStyle+"<input type='text' name='"+name+"' size='"+PString(size)+"' value='"+value+"' style='"+inputStyle+"'";
     if(!readonly) s += "></input></td>"; else s += "readonly></input></td>";
     return s;
   }
   
   PString StringItemUrl(PString name, PString value, int size=12, int readonly=false)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += itemStyle+"<input type='url' name='"+name+"' size='"+PString(size)+"' value='"+value+"' style='"+inputStyle+"'";
     if(!readonly) s += "></input></td>"; else s += "readonly></input></td>";
     return s;
   }
   
   PString IntegerItem(PString name, int value, int min=-2147483647, int max=2147483647, int size=12, int readonly=false)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += itemStyle+"<input name='MIN' value='"+PString(min)+"' type='hidden'>"
                    "<input name='MAX' value='"+PString(max)+"' type='hidden'>"
                    "<input type=number name='"+name+"' size='"+PString(size)+"' min='"+PString(min)+"' max='"+PString(max)+"' value='"+PString(value)+"' style='"+inputStyle+"'";
     if(!readonly) s += "></input></td>"; else s += "readonly></input></td>";
     return s;
   }

   PString BoolItem(PString name, bool value, int readonly=false)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += itemStyle+"<input name='"+name+"' value='FALSE' type='hidden' style='"+inputStyle+"'>"
                    "<input name='"+name+"' value='TRUE' type='checkbox' style='"+inputStyle+"margin-top:12px;margin-bottom:12px;margin-left:3px;'";
     if(value) s +=" checked='yes'";
     if(!readonly) s += "></input></td>"; else s += " readonly></input></td>";
     return s;
   }
   
   PString RadioItem(PString name, bool value, int readonly=false)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += itemStyle+"<input name='"+name+"' value='FALSE' type='hidden' style='"+inputStyle+"'>"
                    "<input name='"+name+"' value='TRUE' type='radio' style='"+inputStyle+"margin-top:12px;margin-bottom:12px;margin-left:3px;'";
     if(value) s +=" checked='yes'";
     if(!readonly) s += "></input></td>"; else s += " readonly></input></td>";
     return s;
   }

   PString SelectItem(PString name, PString value, PString values, int width=120)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     PStringArray data = values.Tokenise(",");
     s += itemStyle+"<select name='"+name+"' width='"+PString(width)+"' style='width:"+PString(width)+"px;"+inputStyle+"'>";
     for(PINDEX i = 0; i < data.GetSize(); i++)
     {
       if(data[i] == value)
         s += "<option selected value='"+data[i]+"'>"+data[i]+"</option>";
       else
         s += "<option value='"+data[i]+"'>"+data[i]+"</option>";
     }
     s +="</select>";
     return s;
   }

   PString HiddenItem(PString name)
   {
     PString id = PString(rand());
     PString s = "<input name='TableItemId' value='"+id+"' type='hidden'>";
     s += "<input name='"+name+"' type='hidden'></input>";
     return s;
   }

   PString NewItemArray(PString name)
   {
     return "<td><table id='"+name+"' cellspacing='0' style='margin-top:-1px;margin-left:-1px;margin-right:1px;'><tbody>";
   }

   PString EndItemArray()
   {
     return "<tr></tr></tbody></table></td>";
   }

   PString StringItemArray(PString name, PString value, int size=12)
   {
     PString s = "<tr>"+rowArrayStyle+"<input type=text name='"+name+"' size='"+PString(size)+"' value='"+value+"' style='"+inputStyle+"'></input>";
     s += "<input type=button value='↑' onClick='rowUp(this,0)' style='"+buttonStyle+"'>";
     s += "<input type=button value='↓' onClick='rowDown(this)' style='"+buttonStyle+"'>";
     s += "<input type=button value='+' onClick='rowClone(this)' style='"+buttonStyle+"'>";
     s += "<input type=button value='-' onClick='rowDelete(this,0)' style='"+buttonStyle+"'>";
     s += "</td>";
     return s;
   }

   PString BeginTable()
   { return "<form method='POST'><div style='overflow-x:auto;overflow-y:hidden;'><table id='table1' cellspacing='8'><tbody>"; }
   
   PString EndTable()
   {
     PString s = "<tr></tr></tbody></table></div><p><input id='button_accept' name='submit' value='Accept' type='submit'><input id='button_reset' name='reset' value='Reset' type='reset'></p></form>";
     s += jsRowDown() + jsRowUp() + jsRowClone()+ jsRowDelete();
     return s;
   }

   PString JsLocale(PString locale)
   {
     return "<script type='text/javascript'>document.write("+locale+");</script>";
   }

   PString buttons()
   {
     PString s;
     if(buttonUp) s += "<input type=button value='↑' onClick='rowUp(this,"+PString(firstEditRow)+")' style='"+buttonStyle+"'>";
     if(buttonDown) s += "<input type=button value='↓' onClick='rowDown(this)' style='"+buttonStyle+"'>";
     if(buttonClone) s += "<input type=button value='+' onClick='rowClone(this)' style='"+buttonStyle+"'>";
     if(buttonDelete) s += "<input type=button value='-' onClick='rowDelete(this,"+PString(firstDeleteRow)+")' style='"+buttonStyle+"'>";
     return s;
   }

   PString jsRowUp() { return "<script type='text/javascript'>\n"
                     "function rowUp(obj,topRow)\n"
                     "{\n"
                     "  var table = obj.parentNode.parentNode.parentNode;\n"
                     "  var rowNum=obj.parentNode.parentNode.sectionRowIndex;\n"
                     "  if(rowNum>topRow) table.rows[rowNum].parentNode.insertBefore(table.rows[rowNum],table.rows[rowNum-1]);\n"
                     "}\n"
                     "</script>\n"; }

   PString jsRowDown() { return "<script type='text/javascript'>\n"
                     "function rowDown(obj)\n"
                     "{\n"
                     "  var table = obj.parentNode.parentNode.parentNode;\n"
                     "  var rowNum = obj.parentNode.parentNode.sectionRowIndex;\n"
                     "  var rows = obj.parentNode.parentNode.parentNode.childNodes.length;\n"
                     "  if(rowNum!=rows-2) table.rows[rowNum].parentNode.insertBefore(table.rows[rowNum+1],table.rows[rowNum]);\n"
                     "}\n"
                     "</script>\n"; }

   PString jsRowClone() { return "<script type='text/javascript'>\n"
                     "function rowClone(obj)\n"
                     "{\n"
                     "  var table = obj.parentNode.parentNode.parentNode.parentNode;\n"
                     "  var rowNum = obj.parentNode.parentNode.sectionRowIndex;\n"
                     "  var node = table.rows[rowNum].cloneNode(true);\n"
                     "  if(table.id == 'table1')\n"
                     "  {\n"
                     "    var seconds = new Date().getTime();\n"
                     "    node.cells[0].childNodes[0].name = seconds\n"
                     "  }\n"
                     "  table.rows[rowNum].parentNode.insertBefore(node, table.rows[rowNum+1]);\n"
                     "}\n"
                     "</script>\n"; }

   PString jsRowDelete() { return "<script type='text/javascript'>\n"
                     "function rowDelete(obj,firstDeleteRow)\n"
                     "{\n"
                     "  var table = obj.parentNode.parentNode.parentNode.parentNode;\n"
                     "  var rowNum = obj.parentNode.parentNode.sectionRowIndex;\n"
                     "  if(table.id == 'table1')\n"
                     "  {\n"
                     "    if(rowNum < firstDeleteRow)\n"
                     "      return;\n"
                     "    table.deleteRow(rowNum);\n"
                     "  } else {\n"
                     "    var table2 = obj.parentNode.parentNode.parentNode;\n"
                     "    var rows = table2.childNodes.length;\n"
                     "    if(rows < 3)\n"
                     "      return;\n"
                     "    table2.deleteRow(rowNum);\n"
                     "  }\n"
                     "}\n"
                     "</script>\n"; }
   bool FormPost(PHTTPRequest & request, const PStringToString & data, PHTML & msg)
   {
     if(msg.IsEmpty())
       msg << "<script>location.href=\"" << request.url.AsString() << "\"</script>";
     return true;
   }
   
   virtual PBoolean Post(
      PHTTPRequest & request,
      const PStringToString & data,
      PHTML & replyMessage
      );
      
   PBoolean OnPOST(
      PHTTPServer & server,
      const PHTTPConnectionInfo & connectInfo
      );
   
   protected:
     PConfig      cfg;
     PStringArray dataArray;
     bool         deleteSection;
     int          firstEditRow,
                  firstDeleteRow,
                  buttonUp, 
                  buttonDown, 
                  buttonClone, 
                  buttonDelete;
   
     PString      separator,
                  colStyle, 
                  rowStyle, 
                  rowArrayStyle, 
                  itemStyle, 
                  itemInfoStyle, 
                  itemInfoStyleRowSpan, 
                  textStyle, 
                  inputStyle, 
                  buttonStyle,
                  columnColor, 
                  rowColor, 
                  itemColor, 
                  itemInfoColor;
};

class GeneralPConfigPage : public TablePConfigPage
{
  public:
    GeneralPConfigPage(OpalMCUEIE & app, const PString & title, 
                                         const PString & section, 
                                         const PHTTPAuthority & auth);
  protected:
    PConfig cfg;
    
};

class H323PConfigPage : public TablePConfigPage
{
  public:
    H323PConfigPage(OpalMCUEIE & app, const PString & title, 
                                      const PString & section, 
                                      const PHTTPAuthority & auth);
  protected:
    PConfig cfg;
    
};

class ConferencePConfigPage : public TablePConfigPage
{
  public:
    ConferencePConfigPage(OpalMCUEIE & app, const PString & title, 
                                            const PString & section, 
                                            const PHTTPAuthority & auth);
  protected:
    PConfig cfg;
};
                                                                

class ControlRoomPage : public PServiceHTTPString                        
{                                                                        
  public:                                                                
    ControlRoomPage(OpalMCUEIE & app, PHTTPAuthority & auth);             
                                                                         
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
    OpalMCUEIE & app;                                                     
}; 

class InteractiveHTTP : public PServiceHTTPString
{
  public:
    InteractiveHTTP(OpalMCUEIE & app, PHTTPAuthority & auth);
    
    PBoolean OnGET (                                                     
      PHTTPServer & server,                                              
      const PHTTPConnectionInfo & connectInfo                            
    );
    
    protected:  
    OpalMCUEIE & app;
};   

class JpegFrameHTTP : public PServiceHTTPString
{
  public:
    JpegFrameHTTP(OpalMCUEIE & app, PHTTPAuthority & auth); 
    
    PBoolean OnGET (                                                     
      PHTTPServer & server,                                              
      const PHTTPConnectionInfo & connectInfo                            
    );  
    
  protected:  
    PDECLARE_MUTEX(mutex);
    OpalMCUEIE & app;
};

class InvitePage : public PServiceHTTPString                             //
{                                                                        //
  public:                                                                //
    InvitePage(OpalMCUEIE & app, PHTTPAuthority & auth);                 //
                                                                         //
    PBoolean OnGET (                                                     //
      PHTTPServer & server,                                              //
      const PHTTPConnectionInfo & connectInfo                            //
    );                                                                   //
    virtual PBoolean Post(                                               //
      PHTTPRequest & request,                                            //
      const PStringToString & data,                                      //
      PHTML & replyMessage                                               //
    );                                                                   //
                                                                         //
  protected:                                                             //
    void CreateHTML(PHTML & html);                                       //
    OpalMCUEIE & app;                                                     //
};                                                                       //

class HomePage : public PServiceHTTPString                               
{                                                                        
  public:                                                                
    HomePage(OpalMCUEIE & app, PHTTPAuthority & auth);                    
                                                                         
  protected:                                                             
    OpalMCUEIE & app;                                                     
};   

#endif
