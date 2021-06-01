#! /usr/bin/env python

import sys
import argparse
import logging

import pydle

log = logging.getLogger(__name__)


class NotifyIRC(pydle.Client):
    def __init__(self, channel, channel_key, notification, use_notice=False, **kwargs):
        super().__init__(**kwargs)
        self.channel = channel if channel.startswith("#") else f"#{channel}"
        self.channel_key = channel_key
        self.notification = notification
        self.use_notice = use_notice
        self.future = None

    async def on_connect(self):
        await super().on_connect()
        if self.use_notice:
            await self.notice(self.channel, self.notification)
            # Need to issue a command and await the response before we quit,
            # otherwise we are disconnected before the notice is processed
            self.future = self.eventloop.create_future()
            await self.rawmsg("VERSION")
            await self.future
            await self.quit()
        else:
            await self.join(self.channel, self.channel_key)

    async def on_join(self, channel, user):
        await super().on_join(channel, user)
        if user != self.nickname:
            return
        await self.message(self.channel, self.notification)
        await self.part(self.channel)

    async def on_part(self, channel, user, message=None):
        await super().on_part(channel, user, message)
        await self.quit()

    async def on_raw_351(self, message):
        """VERSION response"""
        if self.future:
            self.future.set_result(None)


def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", default="chat.freenode.net")
    parser.add_argument("-p", "--port", default=6667, type=int)
    parser.add_argument("--password", default=None, help="Optional server password")
    parser.add_argument("--nickname", default="github-notify")
    parser.add_argument(
        "--sasl-password", help="Nickname password for SASL authentication"
    )
    parser.add_argument("--channel", required=True, help="IRC #channel")
    parser.add_argument("--channel-key", help="IRC #channel password")
    parser.add_argument("--tls", action="store_true")
    parser.add_argument(
        "--notice", action="store_true", help="Use NOTICE instead of PRIVMSG"
    )
    parser.add_argument("--message", required=True)
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args()


def main():
    args = get_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARNING)

    client = NotifyIRC(
        channel=args.channel,
        channel_key=args.channel_key or None,
        notification=args.message,
        use_notice=args.notice,
        nickname=args.nickname,
        sasl_username=args.nickname,
        sasl_password=args.sasl_password or None,
    )
    client.run(
        hostname=args.server,
        port=args.port,
        password=args.password or None,
        tls=args.tls,
        # https://github.com/Shizmob/pydle/pull/84
        # tls_verify=args.tls,
    )


if __name__ == "__main__":
    main()
