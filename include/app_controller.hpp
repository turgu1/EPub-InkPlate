// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __APP_CONTROLLER_HPP__
#define __APP_CONTROLLER_HPP__

#include "global.hpp"
#include "eventmgr.hpp"

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
    enum Ctrl { DIR, PARAM, BOOK, OPTION, LAST };
    
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
     * call the selected controller *enter()* method.
     *  
     * @param new_ctrl The new controller to take control
     */
    void set_controller(Ctrl new_ctrl);

    /**
     * @brief Manage a Key Event
     * 
     * Called when a key is pressed by the user. The method is transfering
     * control to the current controller *key_event()* method.
     * 
     * @param key 
     */
    void key_event(EventMgr::KeyEvent key);

  private:
    static constexpr char const * TAG = "AppController";

    static const int LAST_COUNT = 4;
    Ctrl current_ctrl;
    Ctrl last_ctrl[LAST_COUNT]; ///< LIFO of last controllers in use
};

#if __APP_CONTROLLER__
  AppController app_controller;
#else
  extern AppController app_controller;
#endif

#endif