#ifndef CORE_H
#define CORE_H

#include <QtCore>
#include <QFile>
#include <QString>
#include <QByteArray>
#include <QImage>
#include <QMap>
#include <QLibrary>
#include <QObject>

#include <atomic>

#include "libretro.h"
#include "audiobuffer.h"
#include "logging.h"

/* Core is a class that manages the execution of a Libretro core and its associated game.
 *
 * Core is a state machine, the normal lifecycle goes like this:
 * Core::UNINITIALIZED, Core::READY, Core::FINISHED
 *
 * Core provides signalStateChanged( newState, data ) to inform its controller that its state
 * changed.
 *
 * Contents of data:
 *   Core::UNINITIALIZED: nothing
 *   Core::READY: Data structure containing audio and video timing, format and dimensions
 *   Core::FINISHED: nothing
 *   Core::ERROR: Error enum
 *
 * Call Core's load methods with a valid path to a Libretro core and game, along with controller mappings,
 * then call slotInit() to begin loading the game and slot. Core should change to Core::READY. You
 * may now call slotDoFrame() to have the core emulate a video frame send out signals as data is produced.
 *
 * Currently, neither video nor audio signals are thread-safe. (TODO)
 */

// Helper for resolving libretro methods
#define resolved_sym( name ) symbols.name = ( decltype( symbols.name ) )libretroCore.resolve( #name );

struct LibretroSymbols {

    LibretroSymbols();

    // Libretro core functions
    unsigned( *retro_api_version )( void );
    void ( *retro_cheat_reset )( void );
    void ( *retro_cheat_set )( unsigned , bool , const char * );
    void ( *retro_deinit )( void );
    void *( *retro_get_memory_data )( unsigned );
    size_t ( *retro_get_memory_size )( unsigned );
    unsigned( *retro_get_region )( void );
    void ( *retro_get_system_av_info )( struct retro_system_av_info * );
    void ( *retro_get_system_info )( struct retro_system_info * );
    void ( *retro_init )( void );
    bool ( *retro_load_game )( const struct retro_game_info * );
    bool ( *retro_load_game_special )( unsigned , const struct retro_game_info *, size_t );
    void ( *retro_reset )( void );
    void ( *retro_run )( void );
    bool ( *retro_serialize )( void *, size_t );
    size_t ( *retro_serialize_size )( void );
    void ( *retro_unload_game )( void );
    bool ( *retro_unserialize )( const void *, size_t );

    // Frontend-defined callbacks
    void ( *retro_set_audio_sample )( retro_audio_sample_t );
    void ( *retro_set_audio_sample_batch )( retro_audio_sample_batch_t );
    void ( *retro_set_controller_port_device )( unsigned, unsigned );
    void ( *retro_set_environment )( retro_environment_t );
    void ( *retro_set_input_poll )( retro_input_poll_t );
    void ( *retro_set_input_state )( retro_input_state_t );
    void ( *retro_set_video_refresh )( retro_video_refresh_t );

    // Optional core-defined callbacks
    void ( *retro_audio )();
    void ( *retro_audio_set_state )( bool enabled );
    void ( *retro_frame_time )( retro_usec_t delta );
    void ( *retro_keyboard_event )( bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers );

    void clear() {
        retro_api_version = nullptr;
        retro_cheat_reset = nullptr;
        retro_cheat_set = nullptr;
        retro_deinit = nullptr;
        retro_init = nullptr;
        retro_get_memory_data = nullptr;
        retro_get_memory_size = nullptr;
        retro_get_region = nullptr;
        retro_get_system_av_info = nullptr;
        retro_get_system_info = nullptr;
        retro_load_game = nullptr;
        retro_load_game_special = nullptr;
        retro_reset = nullptr;
        retro_run = nullptr;
        retro_serialize = nullptr;
        retro_serialize_size = nullptr;
        retro_unload_game = nullptr;
        retro_unserialize = nullptr;

        retro_set_audio_sample = nullptr;
        retro_set_audio_sample_batch = nullptr;
        retro_set_controller_port_device = nullptr;
        retro_set_environment = nullptr;
        retro_set_input_poll = nullptr;
        retro_set_input_state = nullptr;
        retro_set_video_refresh = nullptr;

        retro_audio = nullptr;
        retro_audio_set_state = nullptr;
        retro_frame_time = nullptr;
        retro_keyboard_event = nullptr;
    }

};

class Core: public QObject {
        Q_OBJECT

    public:

        Core();
        ~Core();

        typedef enum : int {
            STATEUNINITIALIZED,
            STATEREADY,
            STATEFINISHED,
            STATEERROR
        } State;

        typedef enum : int {

            // Everything's okay!
            CORENOERROR,

            // Unable to load core, file could not be loaded as a shared library?
            // Wrong architecture? Wrong OS? Not even a shared library? File corrupt?
            CORELOAD,

            // The core does not have the right extension for the platform Phoenix is running on
            CORENOTLIBRARY,

            // Unable to load core, file was not found
            CORENOTFOUND,

            // Unable to load core, Phoenix did not have permission to open file
            COREACCESSDENIED,

            // Some other filesystem error preventing core from being loaded
            // IO Error, volume was dismounted, network resource not available
            COREUNKNOWNERROR,

            // Unable to load game, file was not found
            GAMENOTFOUND,

            // Unable to load game, Phoenix did not have permission to open file
            GAMEACCESSDENIED,

            // Some other filesystem error preventing game from being loaded
            // IO Error, volume was dismounted, network resource not available
            GAMEUNKNOWNERROR

        } Error;

        typedef struct {
            retro_system_av_info *avInfo;
            retro_pixel_format pixelFormat;
        } avInfoStruct;

        typedef union {
            Error error;
            avInfoStruct avInfo;
        } stateChangedData;

    public slots:

        // Run core for one frame
        void slotDoFrame();

        // Load a libretro core at the given path
        void slotLoadCore( const char *path );

        // Load a game with the given path
        // It is an error to load a game when a core has not been loaded yet
        void slotLoadGame( const char *path );

    signals:

        void signalStateChanged( State newState, stateChangedData data );

        void signalAudioDataReady( int16_t *data );
        void signalVideoDataReady( uchar *data, unsigned width, unsigned height, size_t pitch );
        void signalFrameRendered();

    protected:

        // Only staticly-linked callbacks may access this data/call these methods

        // A hack that gives us the implicit C++ 'this' pointer while maintaining a C-style function signature
        // for the callbacks as required by libretro.h. Thanks to this, at this time we can only
        // have a single instance of Core running at any time.
        static Core *core;

        // Struct containing libretro methods
        LibretroSymbols symbols;

        // Used by audio callback
        void emitAudioDataReady( int16_t *data );

        // Used by video callback
        void emitVideoDataReady( uchar *data, unsigned width, unsigned height, size_t pitch );

        // Used by environment callback
        QByteArray libraryFilename;
        void emitReadyState();

        // Used by environment callback
        // Info about the OpenGL context provided by the Phoenix frontend
        // for the core's internal use
        retro_hw_render_callback openGLContext;


    private:

        // Wrapper around shared library file (.dll, .dylib, .so)
        QLibrary libretroCore;

        //
        // Core-specific constants
        //

        // Audio and video rates/dimensions/types
        retro_system_av_info *avInfo;
        retro_pixel_format pixelFormat;

        // Information about how the core does stuff
        retro_system_info *systemInfo;
        bool fullPathNeeded;

        // Mapping between a retropad button id and a human-readble (and core-defined) label (a string)
        // For use with controller setting UIs
        // TODO: Make this an array, we'll be getting many of these mappings, each with different button ids/labels
        retro_input_descriptor retropadToStringMap;

        //
        // Paths
        //

        QByteArray systemDirectory;
        QByteArray saveDirectory;
        void setSystemDirectory( QString systemDirectory );
        void setSaveDirectory( QString saveDirectory );

        //
        // Game
        //

        // Raw ROM/ISO data, empty if (fullPathNeeded)
        QByteArray gameData;

        //
        // Audio
        //

        // Buffer pool. Since each buffer holds one frame, depending on core, 30 frames = ~500ms
        int16_t *audioBufferPool[30];
        int audioBufferPoolIndex;

        // Amount audioBufferPool[ audioBufferPoolIndex ] has been filled
        // Each frame, exactly ( sampleRate * 4 ) bytes should be copied to
        // audioBufferPool[ audioBufferPoolIndex ][ audioBufferCurrentByte ], total
        int audioBufferCurrentByte;

        //
        // Video
        //

        // Buffer pool. Depending on core, 30 frames = ~500ms
        uchar *videoBufferPool[30];
        int videoBufferPoolIndex;

        //
        // SRAM
        //

        void *SRAMDataRaw;
        void saveSRAM();
        void loadSRAM();
        // bool saveGameState( QString save_path, QString game_name );
        // bool loadGameState( QString save_path, QString game_name );

        //
        // Callbacks
        //

        static void audioSampleCallback( int16_t left, int16_t right );
        static size_t audioSampleBatchCallback( const int16_t *data, size_t frames );
        static bool environmentCallback( unsigned cmd, void *data );
        static void inputPollCallback( void );
        static void logCallback( enum retro_log_level level, const char *fmt, ... );
        static int16_t inputStateCallback( unsigned port, unsigned device, unsigned index, unsigned id );
        static void videoRefreshCallback( const void *data, unsigned width, unsigned height, size_t pitch );

        //
        // Misc
        //

        // Container class for a libretro core variable
        class Variable {
            public:
                Variable() {} // default constructor

                Variable( const retro_variable *var ) {
                    m_key = var->key;

                    // "Text before first ';' is description. This ';' must be followed by a space,
                    // and followed by a list of possible values split up with '|'."
                    QString valdesc( var->value );
                    int splitidx = valdesc.indexOf( "; " );

                    if( splitidx != -1 ) {
                        m_description = valdesc.mid( 0, splitidx ).toStdString();
                        auto _choices = valdesc.mid( splitidx + 2 ).split( '|' );

                        foreach( auto &choice, _choices ) {
                            m_choices.append( choice.toStdString() );
                        }
                    } else {
                        // unknown value
                    }
                };
                virtual ~Variable() {}

                const std::string &key() const {
                    return m_key;
                };

                const std::string &value( const std::string &default_ ) const {
                    if( m_value.empty() ) {
                        return default_;
                    }

                    return m_value;
                };

                const std::string &value() const {
                    static std::string default_( "" );
                    return value( default_ );
                }

                const std::string &description() const {
                    return m_description;
                };

                const QVector<std::string> &choices() const {
                    return m_choices;
                };

                bool isValid() const {
                    return !m_key.empty();
                };

            private:
                // use std::strings instead of QStrings, since the later store data as 16bit chars
                // while cores probably use ASCII/utf-8 internally..
                std::string m_key;
                std::string m_value; // XXX: value should not be modified from the UI while a retro_run() call happens
                std::string m_description;
                QVector<std::string> m_choices;

        };

        // Core-specific variables
        QMap<std::string, Core::Variable> variables;

};

#endif
