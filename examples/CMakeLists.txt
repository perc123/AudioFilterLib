# Examples subdirectory

add_executable(wav_filter_example
    wav_filter_example.cpp
    ../src/wav_file.cpp
)

target_link_libraries(wav_filter_example
    PRIVATE
        audiofilter
        SndFile::sndfile
)
