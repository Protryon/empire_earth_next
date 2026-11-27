// EEStringIDs.h: named ids of the string table in Language.dll (src/Empire Earth/Language/Language.rc).
// The ids the C++ uses; the whole table, named from the English text (IDS_<id> where the text is not
// nameable), is rust/crates/ee_base/src/string_ids.rs (its test checks these equal it).
// IDS_<RANGE>_FIRST/_COUNT: blocks the code indexes as FIRST + i.
#ifndef EESTRINGIDS_H
#define EESTRINGIDS_H

#define IDS_ARIAL                                                       1      // Arial
#define IDS_TIMES_NEW_ROMAN                                             2      // Times New Roman
#define IDS_COURIER_NEW                                                 3      // Courier New
#define IDS_TAHOMA                                                      4      // Tahoma
#define IDS_ENGLISH                                                     6      // English
#define IDS_TRADE_BUY                                                   5538   // Buy
#define IDS_TRADE_SELL                                                  5539   // Sell
#define IDS_TRADE_FOR                                                   5540   // for
#define IDS_YOU_HAVE_RECEIVED                                           5860   // You have received
#define IDS_IN_INSURANCE                                                5861   //  in insurance
#define IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_SOUND                 5865   // Empire Earth: Art of Conquest requires a sound card and driver in o...
#define IDS_ERROR_CANT_RUN_16EA                                         5866   // ERROR - CAN'T RUN!
#define IDS_FILE_COULDNT_BE_OPENED                                      7152   // The file couldn't be opened.
#define IDS_SAVE_GAME_ISNT_VALID                                        7153   // The save game isn't valid.
#define IDS_SAVE_GAME_INCOMPATIBLE_WITH_THIS_VERSION                    7154   // Save game incompatible with this version of Empire Earth: Art of Co...
#define IDS_SAVE_GAME_CONTAINS_INVALID_DATA                             7156   // Save game contains invalid data and cannot be loaded.
#define IDS_SAVE_GAME_IS_MISSING_DATA_AND_CANNOT                        7157   // Save game is missing data and cannot be loaded.
#define IDS_THERE_WAS_ERROR_READING_TAG                                 7164   // There was an error reading a tag for the campaign file.
#define IDS_SETUP_ALLIES                                                7171   // (Allies)
#define IDS_SETUP_ENEMIES                                               7172   // (Enemies)
#define IDS_SETUP_CUSTOM                                                7173   // (Custom)
#define IDS_SETUP_ALL                                                   7174   // (All)
#define IDS_OBSERVERS                                                   7202   // (Observers)
#define IDS_SETUP_PLAYERS                                               7206   // players
#define IDS_ROTATE                                                      8516   // Rotate
#define IDS_HIDE_UI                                                     8538   // Hide UI
#define IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME                           10001  // Player name already exist in game.  \nPlease choose a unique name.
#define IDS_NO_OPEN_SLOTS_LEFT_IN_GAME                                  10002  // No open slots left in game!  \nPlease choose another game.
#define IDS_GAME_NAME_IS_INVALID                                        10005  // Game Name is Invalid
#define IDS_SERVER_HAS_REMOVED_YOU_FROM_GAME                            10007  // The server has removed you from the game.
#define IDS_TO_JOIN_THIS_GAME_YOU                                       10008  // To join this game you need version %s.\nYou are running version %s....
#define IDS_HOST_OF_THIS_GAME_HAS_GONE                                  10011  // The host of this game has gone away!
#define IDS_COULD_NOT_CONNECT_TO_SELECTED                               10012  // Could not connect to the selected game.  Please check your network ...
#define IDS_IS_INVALID_TOKEN                                            10027  // {%s} is an invalid token
#define IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN                              10028  // (%d) is an invalid effect id for token {%s}
#define IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN                          10029  // {%s} is an invalid effect type to be a token.
#define IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS              10030  // The text file effect name (%s) does not match the scenario's effect...
#define IDS_THERE_ARE_TOO_MANY_PLAYERS_IN_LOBBY                         10034  // There are too many players in the lobby to play this saved game.
#define IDS_PLAYER_DOES_NOT_EXIST_IN_SAVED                              10037  // The Player (%s) does not exist in the saved game.
#define IDS_THIS_SAVED_GAME_REQUIRES_PLAYERS                            10038  // This saved game requires %d players, the lobby contains only %d
#define IDS_THERE_ARE_TOO_MANY_PLAYERS_TO_PLAY                          10039  // There are too many players to play the multiplayer scenario only %d...
#define IDS_THIS_IS_NOT_VALID_MULTIPLAYER_SCENARIO                      10040  // This is not a valid multiplayer scenario file.
#define IDS_THERE_WAS_ERROR_TRYING_TO_LOAD                              10047  // There was an error trying to load an old Scenario file.
#define IDS_ERROR_CREATING_FILE                                         10049  // Error Creating File
#define IDS_ERROR_SAVING_FILE                                           10050  // Error Saving File
#define IDS_YOU_NEED_AT_LEAST_1_HUMAN                                   10058  // You need at least 1 Human Player to play a game
#define IDS_INSUFFICIENT_POWER                                          10062  // Insufficient Power
#define IDS_FILE_TRANSFER_FAILED                                        10064  // File transfer failed.  Please restart game and try again.
#define IDS_GAME_IS_IN_PROGRESS                                         10065  // The Game Is In Progress
#define IDS_COULDNT_FIND_GAME_REQUESTED                                 10066  // Couldn't Find The Game Requested
#define IDS_YOU_MAY_NOT_UNPACK_ALL_UNITS                                10067  // You may not unpack all units at this time
#define IDS_INVALID_FILE_NAME                                           10068  // Invalid file name
#define IDS_LEFT_RIGHT_CLICK_TO_CANCEL_IMPROVEMENT                      11004  // Left/Right Click to cancel %s improvement
#define IDS_LEFT_RIGHT_CLICK_TO_CANCEL_RESEARCH                         11005  // Left/Right Click to cancel %s research
#define IDS_ELEVATION_FILE_TO_USE_WITH_BLANK                            11006  // Elevation File to use with Blank Map
#define IDS_NONE                                                        13001  // <NONE>
#define IDS_COPY                                                        13002  // Copy
#define IDS_EXISTS                                                      13003  // Exists
#define IDS_ALWAYS_TRUE                                                 13004  // Always True
#define IDS_DO_NOTHING                                                  13005  // Do Nothing
#define IDS_OFF_MAP                                                     13006  // Off Map
#define IDS_PREVIOUS_LOCATION                                           13007  // Previous Location
#define IDS_SE_MAP_NONE                                                 20009  // <NONE>
#define IDS_SE_REMOVE                                                   20539  // Remove
#define IDS_SE_HUMAN                                                    20552  // Human
#define IDS_TRIGGER_PLAYER_WORLD                                        29050  // World
#define IDS_ANY_HUMAN                                                   29051  // Any Human
#define IDS_ANY_COMPUTER                                                29052  // Any Computer
#define IDS_ALL_HUMAN                                                   29053  // All Human
#define IDS_ALL_COMPUTER                                                29054  // All Computer
#define IDS_ANY_PLAYER                                                  29055  // Any Player
#define IDS_ANY_EXCLUDE_WORLD                                           29056  // Any Exclude World
#define IDS_TRIGGER_PLAYER                                              29057  // Player %d
#define IDS_FRIENDLY                                                    29513  // Friendly
#define IDS_HOSTILE                                                     29514  // Hostile
#define IDS_CIVILIZATIONS                                               29783  // Civilizations
#define IDS_IMPROVEMENT_ATTACK                                          30000  // Attack
#define IDS_IMPROVEMENT_RANGE                                           30001  // Range
#define IDS_IMPROVEMENT_HIT_POINTS                                      30002  // Hit Points
#define IDS_TRANSPORT                                                   30003  // Transport
#define IDS_IMPROVEMENT_SPEED                                           30005  // Speed
#define IDS_IMPROVEMENT_FLIGHT_TIME                                     30006  // Flight Time
#define IDS_AREA_EFFECT                                                 30007  // Area Effect
#define IDS_SHOCK                                                       30008  // Shock
#define IDS_PIERCE                                                      30009  // Pierce
#define IDS_ARROW                                                       30010  // Arrow
#define IDS_GUN                                                         30011  // Gun
#define IDS_MISSILE                                                     30012  // Missile
#define IDS_LASER                                                       30013  // Laser
#define IDS_IMPROVEMENT_REPAIR                                          30014  // Repair
#define IDS_LEADERSHIP                                                  30015  // Leadership
#define IDS_IMPROVEMENT_GARRISON                                        30023  // Garrison
#define IDS_IMPROVEMENT_POWER                                           30024  // Power
#define IDS_RESOURCE_NAME_FOOD                                          30116  // Food
#define IDS_RESOURCE_NAME_GOLD                                          30119  // Gold
#define IDS_RESOURCE_NAME_IRON                                          30120  // Iron
#define IDS_CARBON                                                      30121  // Carbon
#define IDS_PREHISTORIC_AGE                                             30145  // Prehistoric Age
#define IDS_LOBBY_PASSWORD                                              30205  // Password:
#define IDS_HOST                                                        30226  // Host
#define IDS_JOIN                                                        30227  // Join
#define IDS_COMPUTER                                                    30241  // Computer
#define IDS_SECONDS                                                     30379  // seconds
#define IDS_YOU_ARE_VICTORIOUS                                          30401  // You are Victorious!
#define IDS_YOU_HAVE_BEEN_DEFEATED                                      30402  // You have been Defeated!
#define IDS_PASSWORD_IS_INCORRECT                                       30413  // The password is incorrect.
#define IDS_YOUR_TWO_PASSWORDS_DONT_MATCH                               30420  // Your two passwords don't match
#define IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME                        30425  // Unable to access a server at this time.
#define IDS_GAME_WITH_THAT_NAME_ALREADY_EXISTS                          30430  // A game with that name already exists!
#define IDS_FAILED_TO_CREATE_GAME                                       30432  // Failed to create game
#define IDS_SPECIFIED_GAME_NOT_FOUND                                    30440  // Specified game not found!
#define IDS_SIERRA_COM_USER_IS_ALREADY                                  30441  // A Sierra.com user is already logged in with that User ID.
#define IDS_ONE_OR_MORE_OF_YOUR_FILES                                   30445  // One or more of your files are not the correct version or corrupt.
#define IDS_POP_COST                                                    30469  // Pop Cost
#define IDS_SIERRA_COM_IS_UNABLE_TO_VERIFY                              30474  // Sierra.com is unable to verify your ability to host a game.  You ma...
#define IDS_WHISPERED                                                   30475  //  (whispered):
#define IDS_IS_NOW_IGNORING                                             30476  //  is now ignoring:
#define IDS_IS_NO_LONGER_IGNORING                                       30477  //  is no longer ignoring:
#define IDS_HAS_STARTED_CONSTRUCTING_WONDER                             30478  // %s has started constructing the %s wonder.
#define IDS_GAME_UNKNOWN                                                30491  // Unknown
#define IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME                              30612  // Player '%s' has dropped out of the game!
#define IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT                          30621  // The host is not responding - please wait...
#define IDS_NEW_HOST_ESTABLISHED                                        30634  // New host established.  Continuing the game in progress...
#define IDS_SAVING_GAME_AS                                              30661  // Saving game as '%s' ...
#define IDS_LOADING                                                     30663  // Loading '%s' ...
#define IDS_TIME_LEFT_UNTIL_WINS_WONDER_VICTORY                         31283  // Time left until %s wins a Wonder victory: %d
#define IDS_TRIBUTES_YOU                                                31600  // tributes you
#define IDS_CHANGES_DIPLOMATIC_STANCE_TO                                31601  // changes diplomatic stance to
#define IDS_YOU_TRIBUTE                                                 31602  // You tribute
#define IDS_PLAYER_IS_OUT_OF_SYNCH                                      31708  // Player '%s' is out-of-synch and has been removed from the game.  Ga...
#define IDS_HOST_IS_OUT_OF_SYNCH                                        31709  // Host '%s' is out-of-synch and has been removed from the game.  Game...
#define IDS_YOU_ARE_OUT_OF_SYNCH                                        31710  // You are out-of-synch with the game and have been dropped.  Game wil...
#define IDS_YOU_HAVE_BEEN_BOOTED_DUE                                    31711  // You have been booted due to internet lag.  Game will exit after you...
#define IDS_FORMAT_PERCENT_6_2G                                         31804  // %6.2g%%
#define IDS_CAMPAIGN_IN_PROGRESS_OF_COMPLETED                           31805  // (Campaign in progress; %d of %d completed.)
#define IDS_PLAYER_HAS_RESIGNED_FROM_GAME                               31808  // Player %s has resigned from the game
#define IDS_WARNING_YOU_ARE_RUNNING_OUT                                 31811  // Warning:  You are running out of time to choose a Civilization.
#define IDS_CIVILIZATION_SELECTION_TIME_HAS_EXPIRED                     31812  // Civilization selection time has expired.
#define IDS_MSG_GAME                                                    31813  // --GAME--
#define IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH                         31843  // Player '%s' has a civilization named '%s' which is not compatible w...
#define IDS_FILE_NAME_YOU_SPECIFIED_IS_RESERVED                         31845  // The file name you specified ('%s') is reserved for predefined civil...
#define IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS                         31847  // The campaign %s has a scenario %s which differs from the version in...
#define IDS_CONNECTING_TO_SERVER                                        31848  // Connecting to server...
#define IDS_COULDNT_JOIN_GAME                                           31853  // Couldn't join game.
#define IDS_COULDNT_JOIN_ROOM                                           31854  // Couldn't join room.
#define IDS_SUCCESSFULLY_MODIFIED_YOUR_ACCOUNT                          31857  // Successfully modified your account
#define IDS_COULD_NOT_MODIFY_YOUR_ACCOUNT                               31858  // Could not modify your account
#define IDS_GETTING_CURRENT_USER_INFO                                   31859  // Getting current User Info...
#define IDS_THERE_WAS_ERROR_GETTING_CURRENT_USER                        31860  // There was an error getting current User Info...
#define IDS_JOINING_ROOM                                                31864  // Joining room...
#define IDS_MSG_LOCKED                                                  31868  // (Locked)
#define IDS_MSG_UNLOCKED                                                31869  // (Unlocked)
#define IDS_JUST_IN_TIME_MANUFACTURING_IS_NOT                           31892  // Just-In-Time manufacturing is not available until timer expires.
#define IDS_CIV_POWER_PASSWORD                                          32800  // PASSWORD
#define IDS_PRIVATE                                                     32801  // PRIVATE
#define IDS_IN_PROGRESS                                                 32802  // IN PROGRESS
#define IDS_YOU_CANT_JOIN_THAT_GAME                                     32803  // You can't join that game because it has already started.  Choose a ...
#define IDS_YOU_HAVE_LOST_YOUR_CONNECTION_TO_SIERRA                     32808  // You have lost your connection to Sierra.com!
#define IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY                             32817  // Unable to launch the update utility.  Please exit Empire Earth and ...
#define IDS_SPECIAL_POWERS                                              32850  // Special Powers
#define IDS_ACQUIRED_THROUGH_ADAPTATION                                 32900  // %s acquired through Adaptation.

// ranges indexed by the code: _FIRST + i (_FIRST is the block's first string), _BASE + i (_BASE itself
// is not a string: the code's index starts above 0), _COUNT strings in the block
#define IDS_CIV_BONUS_BASE 29716                                       // civilization builder bonus names: BASE + index (index 1..0x25; 0x26.. are CIV_POWER)
#define IDS_RESOURCE_NAME_BASE 30114                                   // resource + BASE: the resource index counts from 2 (= _FIRST - 2)
#define IDS_CIV_POWER_BASE 32780                                       // civilization power names: BASE + power id (EECivPowersManager::GetName; ids from 0x26)

#endif // EESTRINGIDS_H
