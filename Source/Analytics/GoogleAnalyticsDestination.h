#include "JuceHeader.h"

String getAppVersion();

class GoogleAnalyticsDestination  : public ThreadedAnalyticsDestination
{
public:
    GoogleAnalyticsDestination()
        : ThreadedAnalyticsDestination ("GoogleAnalyticsThread")
    {
        {
            // Choose where to save any unsent events.

            auto appDataDir = File::getSpecialLocation (File::userApplicationDataDirectory)
                                  .getChildFile (JUCEApplication::getInstance()->getApplicationName());

            if (! appDataDir.exists())
                appDataDir.createDirectory();

            savedEventsFile = appDataDir.getChildFile ("analytics_events.xml");
        }

        {
            // It's often a good idea to construct any analytics service API keys
            // at runtime, so they're not searchable in the binary distribution of
            // your application (but we've not done this here). You should replace
            // the following key with your own to get this example application
            // fully working.

            apiKey = "UA-24737717-3";
        }

        startAnalyticsThread (initialPeriodMs);
    }

    ~GoogleAnalyticsDestination()
    {
        // Here we sleep so that our background thread has a chance to send the
        // last lot of batched events. Be careful - if your app takes too long to
        // shut down then some operating systems will kill it forcibly!
        Thread::sleep (initialPeriodMs);
        
        shouldExit = true;
        stopAnalyticsThread (5000);
    }

    int getMaximumBatchSize() override   { return 20; }

    bool logBatchedEvents (const Array<AnalyticsEvent>& events) override
    {
        // Send events to Google Analytics
        
        if(shouldExit) return false;

        StringArray postData;

		String eventType = "event";

        for (auto& event : events)
        {
            DBG("EVENT : " << event.name);
            StringPairArray data;

            if (event.name == "startup")
            {
                data.set ("ec",  "info");
                data.set ("ea",  "appStarted");
				data.set("el", getAppVersion() + " on " + SystemStats::getOperatingSystemName());
				data.set("sc", "start");
				data.set("av", getAppVersion());
				data.set("cd1", getAppVersion());
#if JUCE_DEBUG
				data.set("cd2", "Debug");
#else
				if(Engine::mainEngine != nullptr) data.set("cd2", Engine::mainEngine->isBetaVersion ? "Beta" : "Stable");
#endif
				data.set("cd3", SystemStats::getOperatingSystemName());
			}
            else if (event.name == "shutdown")
            {
                data.set ("ec",  "info");
                data.set ("ea",  "appStopped");
				data.set("sc", "end");
            }
            else if (event.name == "crash")
            {
                data.set ("ec",  "info");
                data.set ("ea",  "appCrashed");
				data.set("sc", "end");
            } else
			{
				LOGWARNING("Unknown analytics event " << event.name);
				data.set("ec", "info");
				data.set("ea", event.name);
				data.set("sc", "start");
			}

			data.set ("cid", event.userID);

            StringArray eventData;

            for (auto& key : data.getAllKeys())
                eventData.add (key + "=" + URL::addEscapeChars (data[key], true));


			String appData("v=1&tid=" + apiKey + "&t=" + eventType + "&");

            postData.add (appData + eventData.joinIntoString ("&"));
        }

		DBG("Analytics :\n" << postData.joinIntoString("\n"));

        auto url = URL ("https://www.google-analytics.com/batch")
                       .withPOSTData (postData.joinIntoString ("\n"));

        {
            const ScopedLock lock (webStreamCreation);

            if (shouldExit)
                return false;

			webStream.reset(new WebInputStream(url, true));
        }

        const auto success = webStream->connect (nullptr);

        // Do an exponential backoff if we failed to connect.
        if (success)
            periodMs = initialPeriodMs;
        else
            periodMs *= 2;

        setBatchPeriod (periodMs);

        return success;
    }

    void stopLoggingEvents() override
    {
        const ScopedLock lock (webStreamCreation);

        shouldExit = true;

        if (webStream != nullptr)
            webStream->cancel();
    }

private:
    void saveUnloggedEvents (const std::deque<AnalyticsEvent>& eventsToSave) override
    {
        // Save unsent events to disk. Here we use XML as a serialisation format, but
        // you can use anything else as long as the restoreUnloggedEvents method can
        // restore events from disk. If you're saving very large numbers of events then
        // a binary format may be more suitable if it is faster - remember that this
        // method is called on app shutdown so it needs to complete quickly!

        XmlDocument previouslySavedEvents (savedEventsFile);
        std::unique_ptr<XmlElement> xml = previouslySavedEvents.getDocumentElement();

		if (xml == nullptr || xml->getTagName() != "events")
			xml.reset(new XmlElement("events"));

        for (auto& event : eventsToSave)
        {
            auto* xmlEvent = new XmlElement ("google_analytics_event");
            xmlEvent->setAttribute ("name", event.name);
            xmlEvent->setAttribute ("timestamp", (int) event.timestamp);
            xmlEvent->setAttribute ("user_id", event.userID);

            auto* parameters = new XmlElement ("parameters");

            for (auto& key : event.parameters.getAllKeys())
                parameters->setAttribute (key, event.parameters[key]);

            xmlEvent->addChildElement (parameters);

            auto* userProperties = new XmlElement ("user_properties");

            for (auto& key : event.userProperties.getAllKeys())
                userProperties->setAttribute (key, event.userProperties[key]);

            xmlEvent->addChildElement (userProperties);

            xml->addChildElement (xmlEvent);
        }

		xml->writeTo(savedEventsFile, {});
    }

    void restoreUnloggedEvents (std::deque<AnalyticsEvent>& restoredEventQueue) override
    {
        XmlDocument savedEvents (savedEventsFile);
        std::unique_ptr<XmlElement> xml = savedEvents.getDocumentElement();

        if (xml == nullptr || xml->getTagName() != "events")
            return;

        const auto numEvents = xml->getNumChildElements();

        for (auto iEvent = 0; iEvent < numEvents; ++iEvent)
        {
            const auto* xmlEvent = xml->getChildElement (iEvent);

            StringPairArray parameters;
            const auto* xmlParameters = xmlEvent->getChildByName ("parameters");
            const auto numParameters = xmlParameters->getNumAttributes();

            for (auto iParam = 0; iParam < numParameters; ++iParam)
                parameters.set (xmlParameters->getAttributeName (iParam),
                                xmlParameters->getAttributeValue (iParam));

            StringPairArray userProperties;
            const auto* xmlUserProperties = xmlEvent->getChildByName ("user_properties");
            const auto numUserProperties = xmlUserProperties->getNumAttributes();

            for (auto iProp = 0; iProp < numUserProperties; ++iProp)
                userProperties.set (xmlUserProperties->getAttributeName (iProp),
                                    xmlUserProperties->getAttributeValue (iProp));

            restoredEventQueue.push_back ({
                xmlEvent->getStringAttribute ("name"),
				0,
                (uint32) xmlEvent->getIntAttribute ("timestamp"),
                parameters,
                xmlEvent->getStringAttribute ("user_id"),
                userProperties
            });
        }

        savedEventsFile.deleteFile();
    }

    const int initialPeriodMs = 1000;
    int periodMs = initialPeriodMs;

    CriticalSection webStreamCreation;
    bool shouldExit = false;
    std::unique_ptr<WebInputStream> webStream;

    String apiKey;

    File savedEventsFile;
};
