#include <asm-generic/errno.h>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <cstring>
#include <opencv2/highgui.hpp>
#include <stdio.h>
#include <mgba/core/core.h>
#include <mgba/feature/commandline.h>
#include <opencv2/opencv.hpp>
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

using namespace cv;
using namespace std;

static int _logLevel = 0;

void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	// We don't need the logging object, so we call UNUSED to ensure there's no warning.
	UNUSED(log);
	// The level parameter is a bitmask that we can easily filter.
	if (level & _logLevel) {
		// Categories are registered at runtime, but the name can be found
		// through a simple lookup.
		printf("%s: ", mLogCategoryName(category));

		// We get a format string and a varargs context from the core, so we
		// need to use the v* version of printf.
		vprintf(format, args);

		// The format strings do NOT include a newline, so we need to
		// append it ourself.
		putchar('\n');
	}
}

int getch() {
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
            perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
            perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            return -1;
        }
    }
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
            perror ("tcsetattr ~ICANON");
    return (buf);
}

int main(int argc, char* argv[]) {
    int flag = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flag | O_NONBLOCK);
    while(1) {
        auto c = getch();
        if (c > 0) {
            cout << c << endl;
        }
        usleep(10000);
    }

    bool didFail = false;
    struct mArguments args = {};
    bool parsed = mArgumentsParse(&args, argc, argv, NULL, 0);
    if (!args.fname) {
		parsed = false;
	}
    if (!parsed || args.showHelp) {
		// If parsing failed, or the user passed --help, show usage.
		usage(argv[0], NULL, NULL, NULL, 0);
		didFail = !parsed;
		return -1;
	}
    if (args.showVersion) {
		// If the user passed --version, show version.
		version(argv[0]);
		return -1;
	}

    // Set up a logger. The default logger prints everything to STDOUT, which is not usually desirable.
	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

    struct mCore* core = mCoreFind(args.fname);
	if (!core) {
		return -1;
	}
    // Initialize the received core.
	core->init(core);
    // Get the dimensions required for this core and send them to the client.
	unsigned width, height;
	core->desiredVideoDimensions(core, &width, &height);
	ssize_t bufferSize = width * height * BYTES_PER_PIXEL;
    Mat frame(height, width, CV_8UC4);
    core->setVideoBuffer(core, (color_t *)frame.data, width);

    // Tell the core to actually load the file.
    mCoreLoadFile(core, args.fname);

    // Initialize the configuration system and load any saved settings for
	// this frontend. The second argument to mCoreConfigInit should either be
	// the name of the frontend, or NULL if you're not loading any saved
	// settings from disk.
    mCoreConfigInit(&core->config, "opencv");
	mCoreConfigLoad(&core->config);

    // Take any settings overrides from the command line and make sure they get
	// loaded into the config system, as well as manually overriding the
	// "idleOptimization" setting to ensure cores that can detect idle loops
	// will attempt the detection.
	mArgumentsApply(&args, NULL, 0, &core->config);
	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");

    // Tell the core to apply the configuration in the associated config object.
	mCoreLoadConfig(core);

    // Set our logging level to be the logLevel in the configuration object.
	mCoreConfigGetIntValue(&core->config, "logLevel", &_logLevel);

    // Reset the core. This is needed before it can run.
	core->reset(core);

    struct timeval tv, tv2, tv_fps;
    gettimeofday(&tv, NULL);
    tv2 = tv;
    tv_fps = tv;
    unsigned frames = 0;
    #define EXPECTED_WAIT 20

    while (1) {
        // expected 16.666 ms
        unsigned dut = (tv.tv_sec - tv2.tv_sec) * 1000 + (tv.tv_usec - tv2.tv_usec) / 1000;
        tv2 = tv;
        // FPS counter
        if ((tv.tv_sec - tv_fps.tv_sec) * 1000000 + (tv.tv_usec - tv_fps.tv_usec) >= 1000000) {
            tv_fps = tv;
            cout << "FPS: " << frames << endl;
            frames = 0;
        }
        if (dut <= EXPECTED_WAIT) {
            auto key = waitKey(EXPECTED_WAIT - dut);
            if (key >= 0) {
                cout << key << endl;
                core->setKeys(core, key);
            } else {
                core->setKeys(core, 0);
            }
        } else {
            core->setKeys(core, 0);
        }
        core->runFrame(core);
        imshow("mGBA", frame);
        frames += 1;
        gettimeofday(&tv, NULL);
    }
    mCoreConfigDeinit(&core->config);
	core->deinit(core);
    return 0;
}
