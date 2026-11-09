// HTML parameter document generation and percent substitution.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLFromText.h"
namespace WONAPI {

// FUNCTION: WON 0x0073F020
// Converts text (or a file) to a document and appends it as a parameter or to the output; false when it fails
bool HTMLDocumentGen::AddDocPrv(const GUIString &theText, int theFlags, bool addToVector)
{
	SmartPtr<HTMLDocument> aDocument;
	HTMLFromText aGen;
	aDocument = aGen.Generate(theText,theFlags);
	
	if(addToVector)
		this->mDocumentVector.push_back(aDocument);
	else
		this->mDocument->AddDocument(aDocument.get());

	return aDocument.get()!=NULL;
}

// FUNCTION: WON 0x0073f170
// Destroys a text-to-HTML converter, releasing its document and pending text
// (??1HTMLFromText@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x0073F230
// Builds the output from a format: text between %N% markers is added as HTML, %N% inserts parameter N, %% a %
void HTMLDocumentGen::printf(const GUIString &theText)
{
	int aStartPos = 0;
	while(true)
	{
		int anEndPos = theText.find('%',aStartPos);
		if(anEndPos<0)
			break;

		if(theText[anEndPos+1]=='%')
		{
			anEndPos++;
			this->AddDoc(theText.substr(aStartPos,anEndPos-aStartPos),HTMLDocGenFlag_IsHTML);
			aStartPos = anEndPos+1;
			continue;
		}
			
		this->AddDoc(theText.substr(aStartPos,anEndPos-aStartPos),HTMLDocGenFlag_IsHTML);

		anEndPos++;
		aStartPos = anEndPos;
		while(anEndPos<theText.length() && theText[anEndPos]!='%')
			anEndPos++;

		GUIString aNumber = theText.substr(aStartPos,anEndPos-aStartPos);
		int aParamNum = aNumber.ToInt()-1;
		if(aParamNum>=0 && aParamNum<this->mDocumentVector.size())
			this->mDocument->AddDocument(this->mDocumentVector[aParamNum].get());

		aStartPos = anEndPos+1;
		if(aStartPos>=theText.length())
			break;
	}

	if(aStartPos<theText.length())
		this->AddDoc(theText.substr(aStartPos),HTMLDocGenFlag_IsHTML);
}


// FUNCTION: WON 0x0073f1f0
// Converts text to a document and appends it to the output
bool HTMLDocumentGen::AddDoc(const GUIString &theText, int theFlags)
{
	return this->AddDocPrv(theText,theFlags,false);
}
// FUNCTION: WON 0x0073f210
// Converts text to a document and stores it as the next printf parameter
bool HTMLDocumentGen::AddDocParam(const GUIString &theText, int theFlags)
{
	return this->AddDocPrv(theText,theFlags,true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x0073efc0
// Creates a document generator writing into the given document (a new empty one when NULL)
HTMLDocumentGen::HTMLDocumentGen(HTMLDocument* document)
{
    this->SetDocument(document);
}
// FUNCTION: WON 0x0073f440
// Sets the output document, creating an empty one when NULL
void HTMLDocumentGen::SetDocument(HTMLDocument* document)
{
    if(document==NULL) this->mDocument=new HTMLDocument(gEmptyGUIString);
    else this->mDocument=document;
}
}

namespace WONAPI {
// Appends a tag to the output document (ignored when NULL; not identified in the original exe)
void HTMLDocumentGen::AddTag(HTMLTag *theTag)
{
	if(theTag!=NULL)
		this->mDocument->AddTag(theTag);
}

// Stores a single tag as the next printf parameter (not identified in the original exe)
void HTMLDocumentGen::AddTagParam(HTMLTag *theTag)
{
	SmartPtr<HTMLDocument> aDoc = new HTMLDocument(gEmptyGUIString);
	if(theTag!=NULL)
		aDoc->AddTag(theTag);

	this->mDocumentVector.push_back(aDoc);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0074cb90
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::HTMLDocument>,int,class WONAPI::SmartPtr<class WONAPI::HTMLDocument> *,class WONAPI::Sm)
// TEMPLATE: WON 0x00750a10
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::HTMLDocument>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTMLD)
