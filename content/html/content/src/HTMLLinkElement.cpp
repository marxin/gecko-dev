
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLLinkElement.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/HTMLLinkElementBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsDOMTokenList.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMStyleSheet.h"
#include "nsINode.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsUnicharUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Link)

namespace mozilla {
namespace dom {

HTMLLinkElement::HTMLLinkElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , Link(MOZ_THIS_IN_INITIALIZER_LIST())
{
}

HTMLLinkElement::~HTMLLinkElement()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLLinkElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLLinkElement,
                                                  nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Traverse(cb);
  tmp->Link::Traverse(cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImportLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLLinkElement,
                                                nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Unlink();
  tmp->Link::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRelList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mImportLoader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLLinkElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLLinkElement, Element)


// QueryInterface implementation for HTMLLinkElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLLinkElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLLinkElement,
                               nsIDOMHTMLLinkElement,
                               nsIStyleSheetLinkingElement,
                               Link)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLLinkElement)

bool
HTMLLinkElement::Disabled()
{
  CSSStyleSheet* ss = GetSheet();
  return ss && ss->Disabled();
}

NS_IMETHODIMP
HTMLLinkElement::GetMozDisabled(bool* aDisabled)
{
  *aDisabled = Disabled();
  return NS_OK;
}

void
HTMLLinkElement::SetDisabled(bool aDisabled)
{
  CSSStyleSheet* ss = GetSheet();
  if (ss) {
    ss->SetDisabled(aDisabled);
  }
}

NS_IMETHODIMP
HTMLLinkElement::SetMozDisabled(bool aDisabled)
{
  SetDisabled(aDisabled);
  return NS_OK;
}


NS_IMPL_STRING_ATTR(HTMLLinkElement, Charset, charset)
NS_IMPL_URI_ATTR(HTMLLinkElement, Href, href)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(HTMLLinkElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLLinkElement, CrossOrigin, crossorigin)

void
HTMLLinkElement::GetItemValueText(nsAString& aValue)
{
  GetHref(aValue);
}

void
HTMLLinkElement::SetItemValueText(const nsAString& aValue)
{
  SetHref(aValue);
}

nsresult
HTMLLinkElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  Link::ResetLinkState(false, Link::ElementHasHref());

  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Link must be inert in ShadowRoot.
  if (aDocument && !GetContainingShadow()) {
    aDocument->RegisterPendingLinkUpdate(this);
  }

  void (HTMLLinkElement::*update)() = &HTMLLinkElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  void (HTMLLinkElement::*updateImport)() = &HTMLLinkElement::UpdateImport;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, updateImport));

  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMLinkAdded"));

  return rv;
}

void
HTMLLinkElement::LinkAdded()
{
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkAdded"));
}

void
HTMLLinkElement::LinkRemoved()
{
  CreateAndDispatchEvent(OwnerDoc(), NS_LITERAL_STRING("DOMLinkRemoved"));
}

void
HTMLLinkElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // If this link is ever reinserted into a document, it might
  // be under a different xml:base, so forget the cached state now.
  Link::ResetLinkState(false, Link::ElementHasHref());

  // Once we have XPCOMGC we shouldn't need to call UnbindFromTree during Unlink
  // and so this messy event dispatch can go away.
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();

  // Check for a ShadowRoot because link elements are inert in a
  // ShadowRoot.
  ShadowRoot* oldShadowRoot = GetBindingParent() ?
    GetBindingParent()->GetShadowRoot() : nullptr;

  if (oldDoc && !oldShadowRoot) {
    oldDoc->UnregisterPendingLinkUpdate(this);
  }
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMLinkRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  UpdateStyleSheetInternal(oldDoc, oldShadowRoot);
  UpdateImport();
}

bool
HTMLLinkElement::ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }

    if (aAttribute == nsGkAtoms::sizes) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLLinkElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                        const nsAString& aEventName)
{
  if (!aDoc)
    return;

  // In the unlikely case that both rev is specified *and* rel=stylesheet,
  // this code will cause the event to fire, on the principle that maybe the
  // page really does want to specify that its author is a stylesheet. Since
  // this should never actually happen and the performance hit is minimal,
  // doing the "right" thing costs virtually nothing here, even if it doesn't
  // make much sense.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::stylesheet, nullptr};

  if (!nsContentUtils::HasNonEmptyAttr(this, kNameSpaceID_None,
                                       nsGkAtoms::rev) &&
      FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::rel,
                      strings, eIgnoreCase) != ATTR_VALUE_NO_MATCH)
    return;

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, aEventName, true, true);
  // Always run async in order to avoid running script when the content
  // sink isn't expecting it.
  asyncDispatcher->PostDOMEvent();
}

void
HTMLLinkElement::UpdateImport()
{
  // 1. link node should be attached to the document.
  nsCOMPtr<nsIDocument> doc = GetCurrentDoc();
  if (!doc) {
    // We might have been just removed from the document, so
    // let's remove ourself from the list of link nodes of
    // the import and reset mImportLoader.
    if (mImportLoader) {
      mImportLoader->RemoveLinkElement(this);
      mImportLoader = nullptr;
    }
    return;
  }

  // Until the script execution order is not sorted out for nested cases
  // let's not allow them.
  if (!doc->IsMasterDocument()) {
    nsContentUtils::LogSimpleConsoleError(
      NS_LITERAL_STRING("Nested imports are not supported yet"),
      "Imports");
    return;
  }

  // 2. rel type should be import.
  nsAutoString rel;
  GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel);
  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel, NodePrincipal());
  if (!(linkTypes & eHTMLIMPORT)) {
    mImportLoader = nullptr;
    return;
  }

  nsCOMPtr<nsIURI> uri = GetHrefURI();
  if (!uri) {
    mImportLoader = nullptr;
    return;
  }

  if (!nsStyleLinkElement::IsImportEnabled(NodePrincipal())) {
    // For now imports are hidden behind a pref...
    return;
  }

  nsRefPtr<ImportManager> manager = doc->ImportManager();
  MOZ_ASSERT(manager, "ImportManager should be created lazily when needed");

  {
    // The load even might fire sooner than we could set mImportLoader so
    // we must use async event and a scriptBlocker here.
    nsAutoScriptBlocker scriptBlocker;
    // CORS check will happen at the start of the load.
    mImportLoader = manager->Get(uri, this, doc);
  }
}

nsresult
HTMLLinkElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                         nsIAtom* aPrefix, const nsAString& aValue,
                         bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);

  // The ordering of the parent class's SetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not set until SetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aName == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    Link::ResetLinkState(!!aNotify, true);
  }

  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::href ||
       aName == nsGkAtoms::rel ||
       aName == nsGkAtoms::title ||
       aName == nsGkAtoms::media ||
       aName == nsGkAtoms::type)) {
    bool dropSheet = false;
    if (aName == nsGkAtoms::rel) {
      uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(aValue,
                                                              NodePrincipal());
      if (GetSheet()) {
        dropSheet = !(linkTypes & nsStyleLinkElement::eSTYLESHEET);
      } else if (linkTypes & eHTMLIMPORT) {
        UpdateImport();
      }
    }

    if (aName == nsGkAtoms::href) {
      UpdateImport();
    }
    
    UpdateStyleSheetInternal(nullptr, nullptr,
                             dropSheet ||
                             (aName == nsGkAtoms::title ||
                              aName == nsGkAtoms::media ||
                              aName == nsGkAtoms::type));
  }

  return rv;
}

nsresult
HTMLLinkElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                           bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);
  // Since removing href or rel makes us no longer link to a
  // stylesheet, force updates for those too.
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::href ||
        aAttribute == nsGkAtoms::rel ||
        aAttribute == nsGkAtoms::title ||
        aAttribute == nsGkAtoms::media ||
        aAttribute == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, nullptr, true);
    }
    if (aAttribute == nsGkAtoms::href ||
        aAttribute == nsGkAtoms::rel) {
      UpdateImport();
    }
  }

  // The ordering of the parent class's UnsetAttr call and Link::ResetLinkState
  // is important here!  The attribute is not unset until UnsetAttr returns, and
  // we will need the updated attribute value because notifying the document
  // that content states have changed will call IntrinsicState, which will try
  // to get updated information about the visitedness from Link.
  if (aAttribute == nsGkAtoms::href && kNameSpaceID_None == aNameSpaceID) {
    Link::ResetLinkState(!!aNotify, false);
  }

  return rv;
}

nsresult
HTMLLinkElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  return PreHandleEventForAnchors(aVisitor);
}

nsresult
HTMLLinkElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return PostHandleEventForAnchors(aVisitor);
}

bool
HTMLLinkElement::IsLink(nsIURI** aURI) const
{
  return IsHTMLLink(aURI);
}

void
HTMLLinkElement::GetLinkTarget(nsAString& aTarget)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::target, aTarget);
  if (aTarget.IsEmpty()) {
    GetBaseTarget(aTarget);
  }
}

nsDOMTokenList* 
HTMLLinkElement::RelList()
{
  if (!mRelList) {
    mRelList = new nsDOMTokenList(this, nsGkAtoms::rel);
  }
  return mRelList;
}

already_AddRefed<nsIURI>
HTMLLinkElement::GetHrefURI() const
{
  return GetHrefURIForAnchors();
}

already_AddRefed<nsIURI>
HTMLLinkElement::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = false;
  nsAutoString href;
  GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);
  if (href.IsEmpty()) {
    return nullptr;
  }
  nsCOMPtr<nsIURI> uri = Link::GetURI();
  return uri.forget();
}

void
HTMLLinkElement::GetStyleSheetInfo(nsAString& aTitle,
                                   nsAString& aType,
                                   nsAString& aMedia,
                                   bool* aIsScoped,
                                   bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsScoped = false;
  *aIsAlternate = false;

  nsAutoString rel;
  GetAttr(kNameSpaceID_None, nsGkAtoms::rel, rel);
  uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(rel, NodePrincipal());
  // Is it a stylesheet link?
  if (!(linkTypes & nsStyleLinkElement::eSTYLESHEET)) {
    return;
  }

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  // If alternate, does it have title?
  if (linkTypes & nsStyleLinkElement::eALTERNATE) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = true;
    }
  }

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  // The HTML5 spec is formulated in terms of the CSSOM spec, which specifies
  // that media queries should be ASCII lowercased during serialization.
  nsContentUtils::ASCIIToLower(aMedia);

  nsAutoString mimeType;
  nsAutoString notUsed;
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);
  nsContentUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");

  return;
}

CORSMode
HTMLLinkElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin)); 
}

EventStates
HTMLLinkElement::IntrinsicState() const
{
  return Link::LinkState() | nsGenericHTMLElement::IntrinsicState();
}

size_t
HTMLLinkElement::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return nsGenericHTMLElement::SizeOfExcludingThis(aMallocSizeOf) +
         Link::SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
HTMLLinkElement::WrapNode(JSContext* aCx)
{
  return HTMLLinkElementBinding::Wrap(aCx, this);
}

already_AddRefed<nsIDocument>
HTMLLinkElement::GetImport()
{
  return mImportLoader ? mImportLoader->GetImport() : nullptr;
}

} // namespace dom
} // namespace mozilla
