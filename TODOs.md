TODO: Transmit sensor readings of the devices via MQTT

TODO: Drop privilege rights after setting realtime scheduler
    - At the moment, the gcoded daemon needs to be executed as root user, if the realtime scheduler shall be used.
      From the security perspective, this is really bad. Therefore implement a logic, which allows the daemon to be started
      as super user, than configure the realtime scheduler and then drop the user rights before executing anything else.
