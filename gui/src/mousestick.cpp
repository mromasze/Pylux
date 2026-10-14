// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "mousestick.h"

#include <QCursor>
#include <QWindow>

#include <algorithm>
#include <cmath>

// Mouse speed (logical pixels per second) that fully tilts the stick at sensitivity 1.0
#define MOUSE_STICK_FULL_TILT_SPEED 3000.0
// Mouse events are sparse and uneven, so speed is averaged over this window
#define MOUSE_STICK_SMOOTHING_NS (25LL * 1000 * 1000)
// After a longer pause (focus change, in-stream menu, cursor re-entering the window)
// tracking restarts from the current position instead of producing a jump
#define MOUSE_STICK_RESYNC_NS (100LL * 1000 * 1000)
// Accumulated motion below this (in pixels) is treated as hand jitter
#define MOUSE_STICK_MIN_MOTION 1.5

void MouseStick::SetParameters(float sensitivity, float deadzone_compensation)
{
	this->sensitivity = std::clamp(sensitivity, 0.01f, 100.0f);
	this->deadzone_compensation = std::clamp(deadzone_compensation, 0.0f, 0.9f);
}

bool MouseStick::Move(QWindow *window, const QPointF &global_pos, qint64 now_ns)
{
	if(!window || !window->isActive())
	{
		tracking = false;
		warp_pending = false;
		return false;
	}

	if(warp_pending)
	{
		warp_pending = false;
		// The move caused by our own warp carries no user motion
		if(std::abs(global_pos.x() - warp_target.x()) < 2.0 && std::abs(global_pos.y() - warp_target.y()) < 2.0)
		{
			last_pos = global_pos;
			last_move_ns = now_ns;
			return false;
		}
		last_pos = warp_target;
	}

	bool moved = false;
	if(tracking && now_ns - last_move_ns < MOUSE_STICK_RESYNC_NS)
	{
		QPointF delta = global_pos - last_pos;
		if(!delta.isNull())
		{
			samples.append({now_ns, delta});
			moved = true;
		}
	}
	else
		window->setCursor(Qt::BlankCursor);
	tracking = true;
	last_pos = global_pos;
	last_move_ns = now_ns;

	// Warp back to the center well before the cursor could leave the window
	const QPoint center = window->mapToGlobal(QPoint(window->width() / 2, window->height() / 2));
	if(std::abs(global_pos.x() - center.x()) > window->width() / 4.0
			|| std::abs(global_pos.y() - center.y()) > window->height() / 4.0)
	{
		QCursor::setPos(window->screen(), center);
		warp_target = center;
		warp_pending = true;
	}
	return moved;
}

bool MouseStick::Update(qint64 now_ns, int16_t *x, int16_t *y)
{
	while(!samples.isEmpty() && now_ns - samples.first().time_ns > MOUSE_STICK_SMOOTHING_NS)
		samples.removeFirst();

	QPointF motion;
	for(const auto &sample : samples)
		motion += sample.delta;

	*x = 0;
	*y = 0;
	if(std::abs(motion.x()) + std::abs(motion.y()) < MOUSE_STICK_MIN_MOTION)
		return !samples.isEmpty();

	const double window_s = MOUSE_STICK_SMOOTHING_NS / 1e9;
	const double vx = motion.x() / window_s;
	const double vy = motion.y() / window_s;
	const double speed = std::hypot(vx, vy);
	const double tilt = std::min(1.0, speed * sensitivity / MOUSE_STICK_FULL_TILT_SPEED);
	const double magnitude = deadzone_compensation + (1.0 - deadzone_compensation) * tilt;
	*x = static_cast<int16_t>(std::lround(vx / speed * magnitude * INT16_MAX));
	*y = static_cast<int16_t>(std::lround(vy / speed * magnitude * INT16_MAX));
	return true;
}
