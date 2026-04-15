/*
  ==============================================================================

    AjazzCommand.cpp
    Created: 7 Apr 2026
    Author:  Andrea Bergamasco [@vjandrea]

  ==============================================================================
*/

#include "Module/ModuleIncludes.h"
#include "AjazzCommand.h"
#include "../AjazzModule.h"

AjazzCommand::AjazzCommand(AjazzModule* _module, CommandContext context, var params, Multiplex* multiplex) :
	BaseCommand(_module, context, params, multiplex),
	ajazzModule(_module),
	row(nullptr),
	column(nullptr),
    sideIndex(nullptr),
	valueParam(nullptr)
{
	action = (AjazzAction)(int)params["action"];

	if (action == SET_COLOR || action == SET_IMAGE || action == SET_TEXT)
	{
		row = addIntParameter("Row", "Row of the button", 1, 1, ajazzModule->numRows);
		column = addIntParameter("Column", "Column of the button", 1, 1, ajazzModule->numColumns);
	}
    else if (action == SET_SIDE_COLOR || action == SET_SIDE_IMAGE || action == SET_SIDE_TEXT)
    {
        sideIndex = addIntParameter("Side Index", "Index of the side icon", 1, 1, ajazzModule->numSideKeys);
    }

	switch (action)
	{
	case SET_COLOR:
    case SET_ALL_COLOR:
    case SET_SIDE_COLOR:
		valueParam = addColorParameter("Color", "Color for the button(s)", Colours::black);
		break;

	case SET_IMAGE:
    case SET_SIDE_IMAGE:
		valueParam = addFileParameter("Image", "Image for the button");
		break;

	case SET_TEXT:
    case SET_SIDE_TEXT:
		valueParam = addStringParameter("Text", "Text for the button", "");
		break;

	case SET_BRIGHTNESS:
		valueParam = addIntParameter("Brightness", "Backlight brightness", 75, 0, 100);
		break;
        
    default: break;
	}
}

AjazzCommand::~AjazzCommand() {}

void AjazzCommand::triggerInternal(int multiplexIndex)
{
	switch (action)
	{
	case SET_COLOR:
		ajazzModule->setColor(row->intValue() - 1, column->intValue() - 1, static_cast<ColorParameter*>(valueParam)->getColor());
		break;

	case SET_ALL_COLOR:
		ajazzModule->setAllColor(static_cast<ColorParameter*>(valueParam)->getColor());
		break;

	case SET_IMAGE:
		ajazzModule->setImage(row->intValue() - 1, column->intValue() - 1, valueParam->stringValue());
		break;

	case SET_TEXT:
		ajazzModule->setText(row->intValue() - 1, column->intValue() - 1, valueParam->stringValue());
		break;

	case CLEAR_TEXTS:
		ajazzModule->clearTexts();
		break;

	case SET_BRIGHTNESS:
		ajazzModule->brightness->setValue(valueParam->intValue());
		break;
        
    case SET_SIDE_COLOR:
        ajazzModule->setSideColor(sideIndex->intValue() - 1, static_cast<ColorParameter*>(valueParam)->getColor());
        break;
        
    case SET_SIDE_IMAGE:
        ajazzModule->setSideImage(sideIndex->intValue() - 1, valueParam->stringValue());
        break;
        
    case SET_SIDE_TEXT:
        ajazzModule->setSideText(sideIndex->intValue() - 1, valueParam->stringValue());
        break;
	}
}
