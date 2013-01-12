/** 
 * @file  LineFiltersList.cpp
 *
 * @brief Implementation for LineFiltersList class.
 */
// ID line follows -- this is updated by SVN
// $Id$

#include "LineFiltersList.h"
#include <vector>
#include <cassert>
#include <cstring>
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "UnicodeString.h"

using std::vector;

/** @brief Registry key for saving linefilters. */
static const TCHAR FiltersRegPath[] =_T("LineFilters");

/**
 * @brief Default constructor.
 */
LineFiltersList::LineFiltersList()
: m_pOptionsMgr(NULL)
{
}

/**
 * @brief Destructor, empties the list.
 */
LineFiltersList::~LineFiltersList()
{
	Empty();
}

/**
 * @brief Add new filter to the list.
 * @param [in] filter Filter string to add.
 * @param [in] enabled Is filter enabled?
 */
void LineFiltersList::AddFilter(const String& filter, bool enabled)
{
	LineFilterItem *item = new LineFilterItem();
	item->enabled = enabled;
	item->filterStr = filter;
	m_items.push_back(item);
}

/**
 * @brief Returns count of items in the list.
 * @return Count of filters in the list.
 */
size_t LineFiltersList::GetCount() const
{
	return m_items.size();
}

/**
 * @brief Empties the list.
 */
void LineFiltersList::Empty()
{
	while (!m_items.empty())
	{
		LineFilterItem * item = m_items.back();
		delete item;
		m_items.pop_back();
	}
}

/**
 * @brief Returns the filter list as one filter string.
 * This function returns the list of filters as one string that can be
 * given to regular expression engine as filter. Filter strings in
 * the list are separated by "|".
 * @return Filter string.
 */
String LineFiltersList::GetAsString() const
{
	String filter;
	vector<LineFilterItem*>::const_iterator iter = m_items.begin();

	while (iter != m_items.end())
	{
		if ((*iter)->enabled)
		{
			if (!filter.empty())
				filter += _T("|");
			filter += (*iter)->filterStr;
		}
		++iter;
	}
	return filter;	
}

/**
 * @brief Return filter from given index.
 * @param [in] ind Index of filter.
 * @return Filter item from the index. If the index is beyond table limit,
 *  return the last item in the list.
 */
const LineFilterItem & LineFiltersList::GetAt(size_t ind) const
{
	if (ind < m_items.size())
		return *m_items[ind];
	else
		return *m_items.back();
}

/**
 * @brief Clone filter list from another list.
 * This function clones filter list from another list. Current items in the
 * list are removed and new items added from the given list.
 * @param [in] list List to clone.
 */
void LineFiltersList::CloneFrom(const LineFiltersList *list)
{
	Empty();
	size_t count = list->GetCount();

	for (size_t i = 0; i < count; i++)
	{
		const LineFilterItem &item = list->GetAt(i);
		AddFilter(item.filterStr.c_str(), item.enabled);
	}
}

/**
 * @brief Compare filter lists.
 * @param [in] list List to compare.
 * @return true if lists are identical, false otherwise.
 */
bool LineFiltersList::Compare(const LineFiltersList *list) const
{
	if (list->GetCount() != GetCount())
		return false;

	for (size_t i = 0; i < GetCount(); i++)
	{
		const LineFilterItem &item1 = list->GetAt(i);
		const LineFilterItem &item2 = GetAt(i);

		if (item1.enabled != item2.enabled)
			return false;

		if (item1.filterStr != item2.filterStr)
			return false;
	}
	return true;
}

/**
 * @brief Read filter list from the options system.
 * @param [in] pOptionsMgr Pointer to options system.
 */
void LineFiltersList::Initialize(COptionsMgr *pOptionsMgr)
{
	assert(pOptionsMgr);
	String valuename(FiltersRegPath);

	m_pOptionsMgr = pOptionsMgr;

	size_t count = m_items.size();
	valuename += _T("/Values");
	m_pOptionsMgr->InitOption(valuename, static_cast<int>(count));
	count = m_pOptionsMgr->GetInt(valuename);

	for (unsigned i = 0; i < count; i++)
	{
		String name = string_format(_T("%s/Filter%02u"), FiltersRegPath, i);
		m_pOptionsMgr->InitOption(name, _T(""));
		String filter = m_pOptionsMgr->GetString(name);

		name = string_format(_T("%s/Enabled%02u"), FiltersRegPath, i);
		m_pOptionsMgr->InitOption(name, (int)true);
		int enabled = m_pOptionsMgr->GetInt(name);
		bool bEnabled = enabled ? true : false;

		AddFilter(filter, bEnabled);
	}
}

/**
 * @brief Save linefilters to options system.
 */
void LineFiltersList::SaveFilters()
{
	assert(m_pOptionsMgr);
	String valuename(FiltersRegPath);

	size_t count = m_items.size();
	valuename += _T("/Values");
	m_pOptionsMgr->SaveOption(valuename, static_cast<int>(count));

	for (size_t i = 0; i < count; i++)
	{
		LineFilterItem *item = m_items[i];

		String name = string_format(_T("%s/Filter%02u"), FiltersRegPath, i);
		m_pOptionsMgr->InitOption(name, _T(""));
		m_pOptionsMgr->SaveOption(name, item->filterStr);

		name = string_format(_T("%s/Enabled%02u"), FiltersRegPath, i);
		m_pOptionsMgr->InitOption(name, 0);
		m_pOptionsMgr->SaveOption(name, (int)item->enabled);
	}

	// Remove options we don't need anymore
	// We could have earlier 10 pcs but now we only need 5
	String filter = string_format(_T("%s/Filter%02u"), FiltersRegPath, count);
	int retval1 = m_pOptionsMgr->RemoveOption(filter);

	filter = string_format(_T("%s/Enabled%02u"), FiltersRegPath, count);
	int retval2 = m_pOptionsMgr->RemoveOption(filter);
	
	while (retval1 == COption::OPT_OK || retval2 == COption::OPT_OK)
	{
		++count;
		filter = string_format(_T("%s/Filter%02u"), FiltersRegPath, count);
		retval1 = m_pOptionsMgr->RemoveOption(filter);
		filter = string_format(_T("%s/Enabled%02u"), FiltersRegPath, count);
		retval2 = m_pOptionsMgr->RemoveOption(filter);
	}
}

/**
 * @brief Import old-style filter string into new system.
 * This function imports old-style (2.6.x and earlier) line filters
 * to new linefilter system. Earlier linefilters were saved as one
 * string to registry.
 * @param [in] filters String containing line filters in old-style.
 */
void LineFiltersList::Import(const String& filters)
{
	const TCHAR sep[] = _T("\r\n");
	TCHAR *p_filters = (TCHAR *)&filters[0];
	TCHAR *token;
	
	if (!filters.empty())
	{
		// find each regular expression and add to list
		token = _tcstok(p_filters, sep);
		while (token)
		{
			AddFilter(token, true);
			token = _tcstok(NULL, sep);
		}
		SaveFilters();
	}
}
