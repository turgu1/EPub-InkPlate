// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

/**
 * @brief Application Controller
 * 
 * Main controller responsible of event transmission to the
 * various controllers of the application.
 * 
 * They are:
 * 
 *   - DIR:    Books directory viewer and selection
 *   - PARAM:  Parameters viewer and selection
 *   - BOOK:   Book content viewer
 *   - OPTION: Application options viewer and edition
 *   - TOC:    Currsent book Table of Content
 * 
 * Each Controller must implements the following methods (No use of abstract class):
 * 
 *   - enter()                            The controller is now receiving control
 *   - leave(bool going_to_deep_sleep)    The controller is loosing control
 *   - input_event(EventMgr::Event event) An input has been received from the user
 * 
 * All controllers are expected to be instanciated at boot time and always available.
 * Any new controller would require to modify the AppController methods to integrate it
 * in the main loop of activities.
 * 
 */
class AppController
{
  public:
    /**
     * @brief 
     * 
     * Used to internally identify the controllers. 
     * 
     * LAST allows for the
     * selection of the last controller in charge before the current one.
     */
    enum class Ctrl { NONE, DIR, PARAM, BOOK, OPTION, TOC, LAST };
    
    AppController();

    /**
     * @brief Start the application
     * 
     * Start the application, giving control to the DIR controller. 
     */
    void start();

    /**
     * @brief Set the controller object
     * 
     * This method will call the current controller *leave()* method then
     * call the selected controller *enter()* method. It is mainly used by
     * the other controllers to direct the next controller that will take 
     * control.
     *  
     * @param new_ctrl The new controller to take control
     */
    void set_controller(Ctrl new_ctrl);

    /**
     * @brief Manage an event
     * 
     * Called when a key or the touch screen is pressed by the user. The method is transfering
     * control to the current controller *input_event()* method.
     * 
     * @param event 
     */
    void input_event(EventMgr::Event event);

    void going_to_deep_sleep();
    void launch();

  private:
    static constexpr char const * TAG = "AppController";

    static const int LAST_COUNT = 4;
    Ctrl current_ctrl;
    Ctrl next_ctrl;
    Ctrl last_ctrl[LAST_COUNT]; ///< LIFO of last controllers in use
};

#if __APP_CONTROLLER__
  AppController app_controller;
#else
  extern AppController app_controller;
#endif
